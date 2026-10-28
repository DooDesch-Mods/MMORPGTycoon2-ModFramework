#pragma once
#include <cstdint>
#include <stdexcept>
#include <string>

namespace mt2fw::mt2::determinism {
// Private experiment: retain one completed window image without re-entering
// native scene drawing, queued GL callbacks, or game/UI update callbacks.
struct WaitRender {
    struct Api {
        unsigned (APIENTRY *error)()=nullptr;
        void (APIENTRY *get)(unsigned,int*)=nullptr;
        unsigned char (APIENTRY *is_enabled)(unsigned)=nullptr;
        void (APIENTRY *enable)(unsigned)=nullptr;
        void (APIENTRY *disable)(unsigned)=nullptr;
        void (APIENTRY *gen_textures)(int,unsigned*)=nullptr;
        void (APIENTRY *bind_texture)(unsigned,unsigned)=nullptr;
        void (APIENTRY *bind_buffer)(unsigned,unsigned)=nullptr;
        void (APIENTRY *tex_parameter)(unsigned,unsigned,int)=nullptr;
        void (APIENTRY *tex_image)(unsigned,int,int,int,int,int,unsigned,unsigned,const void*)=nullptr;
        void (APIENTRY *copy_image)(unsigned,int,int,int,int,int,int,int)=nullptr;
        void (APIENTRY *gen_framebuffers)(int,unsigned*)=nullptr;
        void (APIENTRY *bind_framebuffer)(unsigned,unsigned)=nullptr;
        void (APIENTRY *attach)(unsigned,unsigned,unsigned,unsigned,int)=nullptr;
        unsigned (APIENTRY *complete)(unsigned)=nullptr;
        void (APIENTRY *read_buffer)(unsigned)=nullptr;
        void (APIENTRY *draw_buffer)(unsigned)=nullptr;
        void (APIENTRY *blit)(int,int,int,int,int,int,int,int,unsigned,unsigned)=nullptr;
        bool ready()const{return error&&get&&is_enabled&&enable&&disable&&gen_textures&&bind_texture&&bind_buffer&&tex_parameter&&tex_image&&copy_image&&gen_framebuffers&&bind_framebuffer&&attach&&complete&&read_buffer&&draw_buffer&&blit;}
    } gl;
    static constexpr unsigned read_fb=0x8ca8,draw_fb=0x8ca9,texture_2d=0xde1,
        back=0x405,color_attachment=0x8ce0,scissor=0xc11,srgb=0x8db9;
    uintptr_t window=0,context=0;
    uint32_t thread=0;
    unsigned texture=0,framebuffer=0;
    int width=0,height=0;
    bool valid=false;
    uint64_t captures=0,presents=0;
    struct Saved {
        Api& a;int read=0,draw=0,texture=0,unpack=0,default_read=0,default_draw=0;
        bool clipped=false,encoded=false;
        explicit Saved(Api& value):a(value){
            a.get(0x8caa,&read);a.get(0x8ca6,&draw);a.get(0x8069,&texture);
            a.get(0x88ef,&unpack);
            a.bind_framebuffer(read_fb,0);a.get(0xc02,&default_read);a.bind_framebuffer(read_fb,unsigned(read));
            a.bind_framebuffer(draw_fb,0);a.get(0xc01,&default_draw);a.bind_framebuffer(draw_fb,unsigned(draw));
            clipped=a.is_enabled(scissor)!=0;encoded=a.is_enabled(srgb)!=0;
        }
        ~Saved(){
            a.bind_framebuffer(read_fb,0);a.read_buffer(unsigned(default_read));
            a.bind_framebuffer(draw_fb,0);a.draw_buffer(unsigned(default_draw));
            // Only the default framebuffer's modes were changed. Calling
            // DrawBuffer on an engine MRT would overwrite its other outputs.
            a.bind_framebuffer(read_fb,unsigned(read));
            a.bind_framebuffer(draw_fb,unsigned(draw));
            a.bind_texture(texture_2d,unsigned(texture));
            a.bind_buffer(0x88ec,unsigned(unpack));
            if(clipped)a.enable(scissor);else a.disable(scissor);
            if(encoded)a.enable(srgb);else a.disable(srgb);
        }
    };
    void check(const char* operation){
        const auto code=gl.error();if(code)throw std::runtime_error(std::string("Held framebuffer ")+operation+" GL error="+std::to_string(code));
    }
    void owner(uintptr_t w,uintptr_t c,uint32_t t){
        if(!w||!c||!t)throw std::runtime_error("Held framebuffer requires a current owned window/context/thread");
        if(!gl.ready())throw std::runtime_error("Held framebuffer GL entrypoint unavailable");
        if(window&&(window!=w||context!=c||thread!=t))throw std::runtime_error("Held framebuffer window/context/thread changed");
        window=w;context=c;thread=t;
    }
    void invalidate(){valid=false;}
    void capture(uintptr_t w,uintptr_t c,uint32_t t,int x,int y){
        owner(w,c,t);valid=false;
        if(x<=0||y<=0)throw std::runtime_error("Held framebuffer capture dimensions must be positive");
        check("capture precondition");
        {
            Saved saved(gl);
            gl.bind_framebuffer(read_fb,0);gl.read_buffer(back);
            if(!texture)gl.gen_textures(1,&texture);
            if(!framebuffer)gl.gen_framebuffers(1,&framebuffer);
            if(!texture||!framebuffer)throw std::runtime_error("Held framebuffer allocation returned zero");
            gl.bind_texture(texture_2d,texture);
            if(width!=x||height!=y){
                gl.bind_buffer(0x88ec,0);
                gl.tex_parameter(texture_2d,0x2801,0x2600);gl.tex_parameter(texture_2d,0x2800,0x2600);
                gl.tex_image(texture_2d,0,0x8058,x,y,0,0x1908,0x1401,nullptr);
                check("texture allocation");
                width=x;height=y;
            }
            gl.copy_image(texture_2d,0,0,0,0,0,x,y);
            gl.bind_framebuffer(read_fb,framebuffer);
            gl.attach(read_fb,color_attachment,texture_2d,texture,0);
            if(gl.complete(read_fb)!=0x8cd5)throw std::runtime_error("Held framebuffer is incomplete");
        }
        check("capture/restore");valid=true;++captures;
    }
    void repaint(uintptr_t w,uintptr_t c,uint32_t t,int x,int y){
        owner(w,c,t);
        if(!valid)throw std::runtime_error("Held framebuffer has no completed image for this admission");
        if(x<=0||y<=0)throw std::runtime_error("Held framebuffer repaint dimensions must be positive");
        check("repaint precondition");
        {
            Saved saved(gl);
            gl.bind_framebuffer(read_fb,framebuffer);gl.read_buffer(color_attachment);
            gl.bind_framebuffer(draw_fb,0);gl.draw_buffer(back);
            gl.disable(scissor);gl.disable(srgb);
            gl.blit(0,0,width,height,0,0,x,y,0x4000,0x2600);
        }
        check("repaint/restore");
    }
    void presented(bool ok){if(!ok)throw std::runtime_error("Held framebuffer SDL swap failed");++presents;}
};
}
