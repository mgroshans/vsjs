#ifndef _VSJS_H_
#define _VSJS_H_

#include <napi.h>
#include <VSHelper.h>
#include <VSScript.h>


class Vapoursynth : public Napi::ObjectWrap<Vapoursynth> {
    public:
        static Napi::Object Init(Napi::Env, Napi::Object);
        Vapoursynth(const Napi::CallbackInfo&);
        ~Vapoursynth();

        const VSAPI *vsapi;
        VSScript *se;
        VSNodeRef *node;
        const VSVideoInfo *vi;

    private:
        static Napi::FunctionReference constructor;
        Napi::Value GetInfo(const Napi::CallbackInfo&);
        void GetFrame(const Napi::CallbackInfo&);
};

#endif