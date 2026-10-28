namespace wait_render {
mt2::determinism::WaitRender frame;
bool enabled=false;
uint64_t next_present=0;
std::string error;
std::set<std::string> capture_tokens;
Json last_capture;
template<class T>void resolve(T& out,const char* name){
    out=reinterpret_cast<T>(SDL_GL_GetProcAddress(name));
    if(!out)throw std::runtime_error(std::string("Held framebuffer GL entrypoint unavailable: ")+name);
}
void initialize(){
    auto& a=frame.gl;if(a.ready())return;
    resolve(a.error,"glGetError");resolve(a.get,"glGetIntegerv");resolve(a.is_enabled,"glIsEnabled");
    resolve(a.enable,"glEnable");resolve(a.disable,"glDisable");resolve(a.gen_textures,"glGenTextures");
    resolve(a.bind_texture,"glBindTexture");resolve(a.bind_buffer,"glBindBuffer");resolve(a.tex_parameter,"glTexParameteri");
    resolve(a.tex_image,"glTexImage2D");resolve(a.copy_image,"glCopyTexSubImage2D");
    resolve(a.gen_framebuffers,"glGenFramebuffers");resolve(a.bind_framebuffer,"glBindFramebuffer");
    resolve(a.attach,"glFramebufferTexture2D");resolve(a.complete,"glCheckFramebufferStatus");
    resolve(a.read_buffer,"glReadBuffer");resolve(a.draw_buffer,"glDrawBuffer");resolve(a.blit,"glBlitFramebuffer");
}
void dimensions(SDL_Window* target,int& x,int& y){
    if(!SDL_GetWindowSizeInPixels(target,&x,&y))throw std::runtime_error(std::string("Held framebuffer window dimensions failed: ")+SDL_GetError());
}
void arm(){frame.invalidate();next_present=0;error.clear();}
Json status(){return {{"enabled",enabled},{"valid",frame.valid},{"captures",frame.captures},
    {"presents",frame.presents},{"width",frame.width},{"height",frame.height},{"error",error},{"capture_attempts",capture_tokens.size()},{"capture_receipt",last_capture}};}
void held(){
    if(!enabled||control.phase!=mt2::determinism::Gate::Phase::Held)return;
    const auto now=GetTickCount64();if(now<next_present)return;
    try{
        int x=0,y=0;dimensions(window,x,y);
        frame.repaint(reinterpret_cast<uintptr_t>(window),reinterpret_cast<uintptr_t>(SDL_GL_GetCurrentContext()),GetCurrentThreadId(),x,y);
        const bool ok=original_swap(window);
        if(!ok)throw std::runtime_error(std::string("Held framebuffer SDL swap failed: ")+SDL_GetError());
        frame.presented(ok);next_present=now+16;
    }catch(const std::exception& problem){error=problem.what();throw;}
}
struct CapturePack {
    mt2::determinism::WaitRender::Api& gl;
    int buffer=0,row=0,rows=0,pixels=0,alignment=0;
    explicit CapturePack(mt2::determinism::WaitRender::Api& api):gl(api){
        gl.get(0x88ed,&buffer);gl.get(GL_PACK_ROW_LENGTH,&row);gl.get(GL_PACK_SKIP_ROWS,&rows);
        gl.get(GL_PACK_SKIP_PIXELS,&pixels);gl.get(GL_PACK_ALIGNMENT,&alignment);
        gl.bind_buffer(0x88eb,0);
        glPixelStorei(GL_PACK_ROW_LENGTH,0);glPixelStorei(GL_PACK_SKIP_ROWS,0);glPixelStorei(GL_PACK_SKIP_PIXELS,0);
    }
    ~CapturePack(){
        glPixelStorei(GL_PACK_ROW_LENGTH,row);glPixelStorei(GL_PACK_SKIP_ROWS,rows);
        glPixelStorei(GL_PACK_SKIP_PIXELS,pixels);glPixelStorei(GL_PACK_ALIGNMENT,alignment);
        gl.bind_buffer(0x88eb,unsigned(buffer));
    }
};
Json capture_request(const Json& args){
    const auto token=args.at("token").get<std::string>();
    if(control.phase!=mt2::determinism::Gate::Phase::Held||control.entered||
        args.at("expected_step").get<uint64_t>()!=control.completed)
        throw std::runtime_error("Held image capture requires the exact completed step");
    if(!enabled||!frame.valid||frame.presents==0)
        throw std::runtime_error("Held image capture requires enabled renderer, retained image and confirmed held presentation");
    if(!mt2::determinism::Gate::identifier(token)||capture_tokens.contains(token)||capture_tokens.size()>=4)
        throw std::runtime_error("Held image capture requires unused bounded token and fewer than four process attempts");
    if(!pending_capture.empty())throw std::runtime_error("Held image capture rejected: another screenshot is pending");
    const auto path=confined(root/"diagnostics",token+".bmp");
    if(fs::exists(path))throw std::runtime_error("Held image capture target already exists; no overwrite");
    frame.owner(reinterpret_cast<uintptr_t>(window),reinterpret_cast<uintptr_t>(SDL_GL_GetCurrentContext()),GetCurrentThreadId());
    capture_tokens.insert(token);
    try{
        frame.check("front capture precondition");int x=0,y=0;dimensions(window,x,y);
        {
            mt2::determinism::WaitRender::Saved saved(frame.gl);
            CapturePack pack(frame.gl);
            frame.gl.bind_framebuffer(mt2::determinism::WaitRender::read_fb,0);
            frame.gl.read_buffer(GL_FRONT);
            pending_capture=token;
            capture();
        }
        frame.check("front capture/restore");
        if(!pending_capture.empty())throw std::runtime_error("Held image capture returned without completed screenshot acknowledgement");
        auto receipt=read_json(root/"diagnostics"/"capture.json");
        if(!receipt.is_object()||receipt.at("id")!=token||receipt.at("path")!=path_text(path)||
            receipt.at("width")!=x||receipt.at("height")!=y||!receipt.at("sha256").is_string())
            throw std::runtime_error("Held image capture persisted acknowledgement differs from exact request/path/dimensions");
        const auto sha=receipt.at("sha256").get<std::string>();
        if(sha.size()!=64||sha.find_first_not_of("0123456789abcdef")!=std::string::npos||file_digest(path)!=sha)
            throw std::runtime_error("Held image capture persisted file hash differs from acknowledgement");
        last_capture=receipt;receipt["completed_steps"]=control.completed;receipt["wait_presents"]=frame.presents;
        return receipt;
    }catch(...){
        // capture() clears this only after its atomic ACK write. If that write
        // or validation fails, retain the artifacts but forbid later replay.
        if(pending_capture==token)pending_capture.clear();
        throw;
    }
}
}
void capture_wait_frame(SDL_Window* target){
    if(!wait_render::enabled||!control.entered||
        (control.phase!=mt2::determinism::Gate::Phase::Loading&&control.phase!=mt2::determinism::Gate::Phase::Stepping))return;
    try{
        wait_render::initialize();int x=0,y=0;wait_render::dimensions(target,x,y);
        wait_render::frame.capture(reinterpret_cast<uintptr_t>(target),reinterpret_cast<uintptr_t>(SDL_GL_GetCurrentContext()),GetCurrentThreadId(),x,y);
    }catch(const std::exception& problem){
        wait_render::error=problem.what();
        // Let this already-entered original frame finish once. Its existing
        // observation boundary then reports the failure and forbids replay.
        std::lock_guard lock(observation_gate);if(observation_error.empty())observation_error=wait_render::error;
    }
}
