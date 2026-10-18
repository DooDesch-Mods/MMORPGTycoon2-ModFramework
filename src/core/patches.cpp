#include "mt2fw/patches.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <set>
#include <stdexcept>

namespace mt2fw {
namespace {
using Json = nlohmann::json;
thread_local std::vector<std::pair<const PatchRegistry*,bool>> callback_modes;
bool identifier(const std::string& value) {
    return !value.empty() && value.size()<160 && value.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.-")==std::string::npos;
}
bool target_exists(const std::string& value) { return value=="game.update"||value=="objects.create"||value=="api.time.set"; }
struct CallbackScope {
    CallbackScope(const PatchRegistry* registry,bool observe){callback_modes.emplace_back(registry,observe);}
    ~CallbackScope(){callback_modes.pop_back();}
};
void valid_values(const Json& values) {
    if(!values.is_object())throw std::runtime_error("PATCH_VALUES: api.time.set writer must return an object or nil");
    for(auto it=values.begin();it!=values.end();++it){
        if(it.key()=="paused") {if(!it.value().is_boolean())throw std::runtime_error("PATCH_VALUES: paused must be boolean");}
        else if(it.key()=="speed") {
            if(!it.value().is_number()||!std::isfinite(it.value().get<double>())||it.value().get<double>()<0)
                throw std::runtime_error("PATCH_VALUES: speed must be finite and nonnegative");
        }else throw std::runtime_error("PATCH_VALUES: unknown writable field "+it.key());
    }
}
}
PatchSpec PatchSpec::parse(const Json& value) {
    if(!value.is_object())throw std::runtime_error("PATCH_SPEC: expected an object");
    for(auto it=value.begin();it!=value.end();++it)if(it.key()!="id"&&it.key()!="target"&&it.key()!="mode"&&it.key()!="priority"&&it.key()!="before"&&it.key()!="after")
        throw std::runtime_error("PATCH_SPEC: unknown field "+it.key());
    PatchSpec spec;spec.id=value.at("id").get<std::string>();spec.target=value.at("target").get<std::string>();spec.mode=value.value("mode",std::string("observe"));
    if(value.contains("priority")){
        auto& priority=value.at("priority");
        if(!priority.is_number_integer())throw std::runtime_error("PATCH_SPEC: priority must be an integer");
        if(priority.is_number_unsigned()){
            if(priority.get<uint64_t>()>uint64_t(std::numeric_limits<int>::max()))throw std::runtime_error("PATCH_SPEC: priority exceeds int32 range");
        }else if(priority.get<int64_t>()<std::numeric_limits<int>::min()||priority.get<int64_t>()>std::numeric_limits<int>::max())throw std::runtime_error("PATCH_SPEC: priority exceeds int32 range");
        spec.priority=priority.get<int>();
    }
    for(const char* name:{"before","after"})if(value.contains(name)){
        auto& list=value.at(name);if(!list.is_array()&&!(list.is_object()&&list.empty()))throw std::runtime_error(std::string("PATCH_SPEC: ")+name+" must be an array");
        auto& result=std::string(name)=="before"?spec.before:spec.after;
        for(auto& reference:list)result.push_back(reference.get<std::string>());
    }
    return spec;
}
PatchRegistry::PatchRegistry(Error error):error_(std::move(error)){}
void PatchRegistry::validate_values(const std::string& mode,const Json& values) {
    if(mode=="observe"){if(!values.is_null())throw std::runtime_error("PATCH_READ_ONLY: observe callback must return nil");}
    else if(!values.is_null())valid_values(values);
}
PatchRegistry::Run::Run(Run&& other) noexcept { *this=std::move(other); }
PatchRegistry::Run& PatchRegistry::Run::operator=(Run&& other) noexcept {
    if(this==&other)return *this;
    if(registry_)registry_->end(*this,{{"original_returned",false},{"outcome","unknown"}});
    registry_=other.registry_;other.registry_=nullptr;selected_=std::move(other.selected_);entered_=std::move(other.entered_);
    target_=std::move(other.target_);data=std::move(other.data);failed=other.failed;invocation=other.invocation;generation=other.generation;return *this;
}
PatchRegistry::Run::~Run(){if(registry_)registry_->end(*this,{{"original_returned",false},{"outcome","unknown"}});}
std::vector<std::shared_ptr<PatchRegistry::Entry>> PatchRegistry::candidate(void* owner,const std::string& owner_id,const std::vector<PatchRegistration>& registrations) const {
    if(!owner||!identifier(owner_id))throw std::runtime_error("PATCH_OWNER: invalid authenticated owner");
    std::vector<std::shared_ptr<Entry>> result;
    for(auto& entry:entries_)if(entry->owner!=owner&&entry->owner_id!=owner_id)result.push_back(entry);
    std::set<std::string> ids;
    for(auto& registration:registrations){
        auto& spec=registration.spec;
        if(!identifier(spec.id))throw std::runtime_error("PATCH_ID: invalid patch ID");
        if(!ids.insert(spec.id).second)throw std::runtime_error("PATCH_DUPLICATE: "+owner_id+":"+spec.id);
        if(!target_exists(spec.target))throw std::runtime_error("PATCH_TARGET: unknown target "+spec.target);
        if(spec.mode!="observe"&&spec.mode!="modify")throw std::runtime_error("PATCH_MODE: expected observe or modify");
        if(spec.mode=="modify"&&spec.target!="api.time.set")throw std::runtime_error("PATCH_READ_ONLY: "+spec.target+" supports observe only");
        if(!registration.callbacks.before&&!registration.callbacks.after)throw std::runtime_error("PATCH_CALLBACK: before or after callback required");
        if(spec.mode=="modify"&&!registration.callbacks.before)throw std::runtime_error("PATCH_CALLBACK: modify requires before callback");
        for(auto* references:{&spec.before,&spec.after}){
            if(references->size()>16)throw std::runtime_error("PATCH_BUDGET: ordering references "+std::to_string(references->size())+" exceed 16");
            std::set<std::string> unique;
            for(auto& reference:*references){auto split=reference.find(':');
                if(split==std::string::npos||!identifier(reference.substr(0,split))||!identifier(reference.substr(split+1)))throw std::runtime_error("PATCH_REFERENCE: expected owner:patch, got "+reference);
                if(reference==owner_id+":"+spec.id)throw std::runtime_error("PATCH_CYCLE: self reference "+reference);
                if(!unique.insert(reference).second)throw std::runtime_error("PATCH_REFERENCE: duplicate reference "+reference);
            }
        }
        auto entry=std::make_shared<Entry>();entry->owner=owner;entry->owner_id=owner_id;entry->key=owner_id+":"+spec.id;entry->registration=registration;result.push_back(std::move(entry));
    }
    return result;
}
std::map<std::string,std::vector<std::shared_ptr<PatchRegistry::Entry>>> PatchRegistry::sorted(const std::vector<std::shared_ptr<Entry>>& entries) const {
    std::map<std::string,std::shared_ptr<Entry>> lookup;
    std::map<std::string,std::vector<std::shared_ptr<Entry>>> groups,result;
    for(auto& entry:entries){if(!lookup.emplace(entry->key,entry).second)throw std::runtime_error("PATCH_DUPLICATE: "+entry->key);groups[entry->registration.spec.target].push_back(entry);}
    for(auto& [target,group]:groups){
        if(group.size()>128)throw std::runtime_error("PATCH_BUDGET: "+target+" subscriptions "+std::to_string(group.size())+" exceed 128");
        std::string writer;
        std::map<std::string,std::set<std::string>> edges;
        std::map<std::string,size_t> degree;
        for(auto& entry:group){degree[entry->key]=0;if(entry->registration.spec.mode=="modify"){
            if(!writer.empty())throw std::runtime_error("PATCH_WRITER_CONFLICT: "+target+" paused/speed owned by "+writer+"; rejected "+entry->key);writer=entry->key;
        }}
        for(auto& entry:group)for(bool before:{true,false})for(auto& reference:before?entry->registration.spec.before:entry->registration.spec.after){
            auto found=lookup.find(reference);if(found==lookup.end())continue;
            if(found->second->registration.spec.target!=target)throw std::runtime_error("PATCH_REFERENCE_TARGET: "+entry->key+" references "+reference+" on another target");
            auto from=before?entry->key:reference,to=before?reference:entry->key;
            if(edges[from].insert(to).second)++degree[to];
        }
        auto earlier=[](const std::shared_ptr<Entry>& a,const std::shared_ptr<Entry>& b){
            if(a->registration.spec.priority!=b->registration.spec.priority)return a->registration.spec.priority>b->registration.spec.priority;
            if(a->owner_id!=b->owner_id)return a->owner_id<b->owner_id;
            return a->registration.spec.id<b->registration.spec.id;
        };
        auto& ordered=result[target];
        while(ordered.size()<group.size()){
            std::vector<std::shared_ptr<Entry>> ready;
            for(auto& entry:group)if(degree[entry->key]==0)ready.push_back(entry);
            if(ready.empty()){
                std::map<std::string,int> colors;std::vector<std::string> path,cycle;
                std::function<bool(const std::string&)> visit=[&](const std::string& key){
                    colors[key]=1;path.push_back(key);
                    for(auto& to:edges[key])if(degree[to]!=size_t(-1)){
                        if(colors[to]==1){auto start=std::find(path.begin(),path.end(),to);cycle.assign(start,path.end());cycle.push_back(to);return true;}
                        if(!colors[to]&&visit(to))return true;
                    }
                    path.pop_back();colors[key]=2;return false;
                };
                for(auto& [key,count]:degree)if(count!=size_t(-1)&&!colors[key]&&visit(key))break;
                std::string cause="PATCH_CYCLE: "+target+"; ";for(size_t i=0;i<cycle.size();++i){if(i)cause+=" -> ";cause+=cycle[i];}
                throw std::runtime_error(cause+"; previous generation "+std::to_string(generation_)+" retained");
            }
            std::sort(ready.begin(),ready.end(),earlier);auto next=ready.front();ordered.push_back(next);degree[next->key]=size_t(-1);
            for(auto& to:edges[next->key])--degree[to];
        }
    }
    return result;
}
void PatchRegistry::validate(void* owner,const std::string& owner_id,const std::vector<PatchRegistration>& registrations) const {
    std::lock_guard lock(gate_);sorted(candidate(owner,owner_id,registrations));
}
void PatchRegistry::publish(std::vector<std::shared_ptr<Entry>> entries){auto order=sorted(entries);entries_=std::move(entries);order_=std::move(order);++generation_;}
void PatchRegistry::replace(void* owner,const std::string& owner_id,const std::vector<PatchRegistration>& registrations){
    std::lock_guard lock(gate_);publish(candidate(owner,owner_id,registrations));
}
void PatchRegistry::add(void* owner,const std::string& owner_id,PatchSpec spec,PatchCallbacks callbacks){
    std::lock_guard lock(gate_);std::vector<PatchRegistration> registrations;
    for(auto& entry:entries_)if(entry->owner==owner)registrations.push_back(entry->registration);
    registrations.push_back({std::move(spec),std::move(callbacks)});publish(candidate(owner,owner_id,registrations));
}
bool PatchRegistry::remove(void* owner,const std::string& id){
    std::lock_guard lock(gate_);auto entries=entries_;auto count=std::erase_if(entries,[&](auto& entry){return entry->owner==owner&&entry->registration.spec.id==id;});
    if(!count)throw std::runtime_error("PATCH_NOT_FOUND: patch ID is not owned: "+id);
    publish(std::move(entries));return !busy(owner);
}
bool PatchRegistry::retire(void* owner){
    std::lock_guard lock(gate_);auto entries=entries_;if(std::erase_if(entries,[&](auto& entry){return entry->owner==owner;}))publish(std::move(entries));return !busy(owner);
}
bool PatchRegistry::busy(void* owner) const {std::lock_guard lock(gate_);auto found=inflight_.find(owner);return found!=inflight_.end()&&found->second!=0;}
bool PatchRegistry::has_subscribers(const std::string& target) const {
    std::lock_guard lock(gate_);auto found=order_.find(target);return found!=order_.end()&&!found->second.empty();
}
PatchRegistry::Json PatchRegistry::targets() const {
    return Json::array({{{"id","game.update"},{"origin","native"},{"phases",{"before","after"}},{"modes",{"observe"}},{"lua_thread","game"}},
        {{"id","objects.create"},{"origin","native"},{"phases",{"before","after"}},{"modes",{"observe"}},{"lua_thread","game"}},
        {{"id","api.time.set"},{"origin","framework_request"},{"phases",{"before","after"}},{"modes",{"observe","modify"}},{"write_slot","paused/speed"},{"authority","required"}}});
}
PatchRegistry::Json PatchRegistry::status() const {
    std::lock_guard lock(gate_);Json result={{"generation",std::to_string(generation_)},{"targets",targets()},{"patches",Json::array()}};
    std::set<std::string> ids;for(auto& entry:entries_)ids.insert(entry->key);
    for(auto& [target,order]:order_)for(size_t i=0;i<order.size();++i){auto& entry=order[i];auto& spec=entry->registration.spec;Json missing=Json::array();
        for(auto* list:{&spec.before,&spec.after})for(auto& ref:*list)if(!ids.contains(ref))missing.push_back(ref);
        result["patches"].push_back({{"id",spec.id},{"owner",entry->owner_id},{"key",entry->key},{"target",target},{"mode",spec.mode},{"priority",spec.priority},{"order",i},{"unresolved",missing},{"last_error",entry->last_error},{"suppressed_errors",entry->suppressed_errors}});
    }
    return result;
}
void PatchRegistry::report(Entry& entry,const char* phase,const std::string& cause) noexcept {
    auto message="Patch "+entry.key+" "+entry.registration.spec.target+" "+phase+" failed: "+cause;
    try{{std::lock_guard lock(gate_);if(entry.last_error==message){++entry.suppressed_errors;return;}entry.last_error=message;}
        if(error_)error_(entry.owner_id,message);else std::fprintf(stderr,"%s\n",message.c_str());}
    catch(const std::exception& problem){std::fprintf(stderr,"%s; logging failed: %s\n",message.c_str(),problem.what());}
    catch(...){std::fprintf(stderr,"%s; logging failed with unknown exception\n",message.c_str());}
}
bool PatchRegistry::in_observer() const {for(auto& [registry,observe]:callback_modes)if(registry==this&&observe)return true;return false;}
bool PatchRegistry::in_callback() const {for(auto& context:callback_modes)if(context.first==this)return true;return false;}
PatchRegistry::Run PatchRegistry::begin(const std::string& target,Json data,const void* native,bool allow_modify){
    if(!target_exists(target))throw std::runtime_error("PATCH_TARGET: unknown target "+target);
    if(!data.is_object())throw std::runtime_error("PATCH_PAYLOAD: expected an object");
    if(callback_modes.size()>=64)throw std::runtime_error("PATCH_DEPTH: callback nesting 64 reached limit 64");
    Run run;run.registry_=this;run.target_=target;run.data=std::move(data);
    {std::lock_guard lock(gate_);run.invocation=next_invocation_++;run.generation=generation_;auto found=order_.find(target);
        if(found!=order_.end())run.selected_=found->second;
        for(auto& entry:run.selected_)++inflight_[entry->owner];
    }
    run.data["invocation"]=std::to_string(run.invocation);run.data["generation"]=std::to_string(run.generation);run.data["target"]=target;run.data["phase"]="before";
    for(auto& entry:run.selected_){auto& registration=entry->registration;
        try{
            if(registration.spec.mode=="modify"&&!allow_modify)throw std::runtime_error("PATCH_AUTHORITY_UNKNOWN: mutable target requires acknowledged authority");
            if(registration.callbacks.before){CallbackScope scope(this,registration.spec.mode=="observe");auto values=registration.callbacks.before({run.data,native});
                validate_values(registration.spec.mode,values);
                if(registration.spec.mode=="modify"&&!values.is_null())for(auto it=values.begin();it!=values.end();++it)run.data[it.key()]=it.value();
            }
            run.entered_.push_back(entry);
        }catch(const std::exception& problem){report(*entry,"before",problem.what());if(registration.spec.mode=="modify")run.failed=true;}
        catch(...){report(*entry,"before","unknown native exception");if(registration.spec.mode=="modify")run.failed=true;}
    }
    return run;
}
void PatchRegistry::end(Run& run,Json after,const void* native) noexcept {
    if(run.registry_!=this)return;
    try{
        if(after.is_object())for(auto it=after.begin();it!=after.end();++it)run.data[it.key()]=it.value();
        run.data["phase"]="after";
        for(auto it=run.entered_.rbegin();it!=run.entered_.rend();++it){auto& entry=**it;
            if(entry.registration.callbacks.after)try{CallbackScope scope(this,entry.registration.spec.mode=="observe");entry.registration.callbacks.after({run.data,native});}
            catch(const std::exception& problem){report(entry,"after",problem.what());}catch(...){report(entry,"after","unknown native exception");}
        }
    }catch(const std::exception& problem){std::fprintf(stderr,"Patch completion failed: %s\n",problem.what());}
    catch(...){std::fprintf(stderr,"Patch completion failed with unknown native exception\n");}
    {std::lock_guard lock(gate_);std::vector<void*> owners;for(auto& entry:run.selected_)owners.push_back(entry->owner);
        run.registry_=nullptr;run.entered_.clear();run.selected_.clear();
        for(auto owner:owners){auto found=inflight_.find(owner);if(found!=inflight_.end()&&!--found->second)inflight_.erase(found);}
    }
}
}
