#include <sstream>
#include <node_buffer.h>
#include "vsjs.h"

using v8::Local;
using v8::Value;
using v8::Handle;
using v8::Number;
using v8::Object;
using v8::String;
using v8::Function;
using v8::FunctionTemplate;

using node::Buffer::Data;

using std::ostringstream;

Nan::Persistent<Function> Vapoursynth::constructor;

Vapoursynth::Vapoursynth(const char *script, const char *path) {
    vsapi = vsscript_getVSApi();
    assert(vsapi);

    se = NULL;
    if (vsscript_evaluateScript(&se, script, path, efSetWorkingDir)) {
        const char *error = vsscript_getError(se);
        vsscript_freeScript(se);
        se = NULL;

        ostringstream ss;
        ss << "Failed to evaluate " << path;
        if (error) {
            ss << ": " << error;
        }

        Nan::ThrowError(ss.str().data());
        return;
    }

    node = vsscript_getOutput(se, 0);
    if (!node) {
       vsscript_freeScript(se);
       se = NULL;
        Nan::ThrowError("Failed to retrieve output node\n");
        return;
    }

    vi = vsapi->getVideoInfo(node);

    if (!isConstantFormat(vi) || !vi->numFrames) {
        vsapi->freeNode(node);
        vsscript_freeScript(se);
        se = NULL;
        Nan::ThrowError("Cannot output clips with varying dimensions or unknown length\n");
        return;
    }
}

Vapoursynth::~Vapoursynth() {
    if (se != NULL) {
       vsscript_freeScript(se);
    }
}

void Vapoursynth::Init(Handle<Object> exports) {
    Local<FunctionTemplate> tpl = Nan::New<FunctionTemplate>(New);
    tpl->SetClassName(Nan::New<String>("Vapoursynth").ToLocalChecked());
    tpl->InstanceTemplate()->SetInternalFieldCount(1);
    Nan::SetPrototypeMethod(tpl, "getInfo", GetInfo);
    Nan::SetPrototypeMethod(tpl, "getFrame", GetFrame);
    constructor.Reset(Nan::GetFunction(tpl).ToLocalChecked());
    Nan::Set(exports, Nan::New<String>("Vapoursynth").ToLocalChecked(), Nan::GetFunction(tpl).ToLocalChecked());

    if (!vsscript_init()) {
        Nan::ThrowError("Failed to initialize Vapoursynth environment");
        return;
    }
}

NAN_METHOD(Vapoursynth::New) {
    if (info.IsConstructCall()) {
        // Invoked as constructor: `new Vapoursynth(...)`
        const int32_t scriptLen = Nan::Get(info[0].As<Object>(), Nan::New<String>("length").ToLocalChecked()).ToLocalChecked()->Int32Value();
        char *script = Data(info[0]);
        script[scriptLen] = '\0';
        const Nan::Utf8String utf8path(info[1]);
        const char *path(*utf8path);
        Vapoursynth *obj = new Vapoursynth(script, path);
        obj->Wrap(info.This());
        info.GetReturnValue().Set(info.This());
    } else {
        // Invoked as plain function `Vapoursynth(...)`, turn into construct call
        const int argc = 2;
        Local<Value> argv[argc] = { info[0], info[1] };
        Local<Function> cons = Nan::New<Function>(constructor);
        info.GetReturnValue().Set(cons->NewInstance(argc, argv));
    }
}

class FrameWorker : public Nan::AsyncWorker {
    public:
        FrameWorker(Vapoursynth *obj, int32_t frameNumber, Local<Object> bufferHandle, Nan::Callback *callback)
                : obj(obj), frameNumber(frameNumber), outBuffer(Data(bufferHandle)), Nan::AsyncWorker(callback) {
            SaveToPersistent("bufferHandle", bufferHandle);
        }
        ~FrameWorker() {}

        void Execute() {
            char errMsg[1024];
            const VSFrameRef *frame = obj->vsapi->getFrame(frameNumber, obj->node, errMsg, sizeof(errMsg));

            if (!frame) {
                ostringstream ss;
                ss << "Encountered error getting frame " << frameNumber << ": " << errMsg;
                SetErrorMessage(ss.str().data());
                return;
            }

            for (int p = 0; p < obj->vi->format->numPlanes; p++) {
                int stride = obj->vsapi->getStride(frame, p);
                const uint8_t *readPtr = obj->vsapi->getReadPtr(frame, p);
                int rowSize = obj->vsapi->getFrameWidth(frame, p) * obj->vi->format->bytesPerSample;
                int height = obj->vsapi->getFrameHeight(frame, p);

                for (int y = 0; y < height; y++) {
                    memcpy(outBuffer, readPtr, rowSize);
                    outBuffer += rowSize;
                    readPtr += stride;
                }
            }

            obj->vsapi->freeFrame(frame);
        }

    protected:
        void HandleOKCallback() {
            v8::Local<v8::Value> argv[] = {
                Nan::Null(),
                Nan::New<Number>(frameNumber),
                GetFromPersistent("bufferHandle")
            };
            callback->Call(3, argv);
        }

        void HandleErrorCallback() {
            v8::Local<v8::Value> argv[] = {
                v8::Exception::Error(Nan::New<String>(ErrorMessage()).ToLocalChecked()),
                Nan::New<Number>(frameNumber),
                GetFromPersistent("bufferHandle")
            };
            callback->Call(3, argv);
        }

    private:
        Vapoursynth *obj;
        int32_t frameNumber;
        char *outBuffer;
};

NAN_METHOD(Vapoursynth::GetFrame) {
    Vapoursynth *obj = Unwrap<Vapoursynth>(info.This());
    int32_t frameNumber = info[0]->Int32Value();
    Local<Object> bufferHandle = info[1].As<v8::Object>();
    Nan::Callback *callback = new Nan::Callback(info[2].As<Function>());
    Nan::AsyncQueueWorker(new FrameWorker(obj, frameNumber, bufferHandle, callback));
}

static int frameSize(const VSVideoInfo *vi) {
    if (!vi) {
        return 0;
    }

    int frame_size = (vi->width * vi->format->bytesPerSample) >> vi->format->subSamplingW;
    if (frame_size) {
        frame_size *= vi->height;
        frame_size >>= vi->format->subSamplingH;
        frame_size *= 2;
    }
    frame_size += vi->width * vi->format->bytesPerSample * vi->height;

    return frame_size;
}

NAN_METHOD(Vapoursynth::GetInfo) {
    Vapoursynth *obj = Unwrap<Vapoursynth>(info.This());

    Local<Object> ret = Nan::New<Object>();
    Nan::Set(ret, Nan::New<String>("height").ToLocalChecked(), Nan::New<Number>(obj->vi->height));
    Nan::Set(ret, Nan::New<String>("width").ToLocalChecked(), Nan::New<Number>(obj->vi->width));
    Nan::Set(ret, Nan::New<String>("numFrames").ToLocalChecked(), Nan::New<Number>(obj->vi->numFrames));

    Local<Object> fps = Nan::New<Object>();
    Nan::Set(fps, Nan::New<String>("numerator").ToLocalChecked(), Nan::New<Number>(obj->vi->fpsNum));
    Nan::Set(fps, Nan::New<String>("denominator").ToLocalChecked(), Nan::New<Number>(obj->vi->fpsDen));
    Nan::Set(ret, Nan::New<String>("fps").ToLocalChecked(), fps);

    Nan::Set(ret, Nan::New<String>("frameSize").ToLocalChecked(), Nan::New<Number>(frameSize(obj->vi)));

    info.GetReturnValue().Set(ret);
}