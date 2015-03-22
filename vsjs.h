#ifndef VSJS_H
#define VSJS_H

#include <node.h>
#include <nan.h>
#include "VSHelper.h"
#include "VSScript.h"

class Vapoursynth : public Nan::ObjectWrap {
    public:
        static void Init(v8::Handle<v8::Object> exports);

        const VSAPI *vsapi;
        VSScript *se;
        VSNodeRef *node;
        const VSVideoInfo *vi;

    private:
        Vapoursynth(const char*, const char*);
        ~Vapoursynth();

        static NAN_METHOD(New);
        static NAN_METHOD(GetInfo);
        static NAN_METHOD(GetFrame);
        static Nan::Persistent<v8::Function> constructor;
};

#endif