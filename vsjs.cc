#include <sstream>
#include "vsjs.h"

using std::ostringstream;

Napi::Object Vapoursynth::Init(Napi::Env env, Napi::Object exports) {
    Napi::Function func = DefineClass(env, "Vapoursynth", {
        InstanceMethod("getInfo", &Vapoursynth::GetInfo),
        InstanceMethod("getFrame", &Vapoursynth::GetFrame)
    });

    constructor = Napi::Persistent(func);
    constructor.SuppressDestruct();
    exports.Set("Vapoursynth", func);
    
    if (!vsscript_init()) {
        Napi::Error::New(env, "Failed to initialize Vapoursynth environment").ThrowAsJavaScriptException();
    }

    return exports;
}

Vapoursynth::Vapoursynth(const Napi::CallbackInfo& info) : Napi::ObjectWrap<Vapoursynth>(info) {
    Napi::Env env = info.Env();

    Napi::Buffer<char> scriptBuffer = info[0].As<Napi::Buffer<char>>();
    char *script = scriptBuffer.Data();
    const char *path = info[1].As<Napi::String>().Utf8Value().c_str();

    vsapi = vsscript_getVSApi2(VAPOURSYNTH_API_VERSION);
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

        Napi::Error::New(env, ss.str()).ThrowAsJavaScriptException();
        return;
    }

    node = vsscript_getOutput(se, 0);
    if (!node) {
        vsscript_freeScript(se);
        se = NULL;
        Napi::Error::New(env, "Failed to retrieve output node\n").ThrowAsJavaScriptException();
        return;
    }

    vi = vsapi->getVideoInfo(node);

    if (!isConstantFormat(vi) || !vi->numFrames) {
        vsapi->freeNode(node);
        vsscript_freeScript(se);
        se = NULL;
        Napi::Error::New(env, "Cannot output clips with varying dimensions or unknown length\n").ThrowAsJavaScriptException();
        return;
    }
}

Vapoursynth::~Vapoursynth() {
    if (se != NULL) {
       vsscript_freeScript(se);
    }
}

Napi::FunctionReference Vapoursynth::constructor;

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

Napi::Value Vapoursynth::GetInfo(const Napi::CallbackInfo &info) {
    Napi::Env env = info.Env();

    Napi::Object scriptInfo = Napi::Object::New(env);

    scriptInfo.Set("height", vi->height);
    scriptInfo.Set("width", vi->width);
    scriptInfo.Set("numFrames", vi->numFrames);

    Napi::Object fps = Napi::Object::New(env);
    fps.Set("numerator", vi->fpsNum);
    fps.Set("denominator", vi->fpsDen);

    scriptInfo.Set("fps", fps);

    scriptInfo.Set("frameSize", frameSize(vi));

    return scriptInfo;
}

class FrameWorker : public Napi::AsyncWorker {
    public:
        FrameWorker(Vapoursynth *obj, int32_t frameNumber, Napi::Buffer<char> &buffer, Napi::Function &callback)
            : obj(obj), frameNumber(frameNumber), bufferHandle(Napi::Persistent(buffer)), outBuffer(buffer.Data()), Napi::AsyncWorker(callback) {}

        ~FrameWorker() {
            bufferHandle.Unref();
        }

        void Execute() {
            char errMsg[1024];
            const VSFrameRef *frame = obj->vsapi->getFrame(frameNumber, obj->node, errMsg, sizeof(errMsg));

            if (!frame) {
                ostringstream ss;
                ss << "Encountered error getting frame " << frameNumber << ": " << errMsg;
                SetError(ss.str());
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
        std::vector<napi_value> GetResult(Napi::Env env) {
            return {
                env.Null(),
                Napi::Number::New(env, frameNumber),
                bufferHandle.Value()
            };
        }

    private:
        Vapoursynth *obj;
        int32_t frameNumber;
        char *outBuffer;
        Napi::Reference<Napi::Buffer<char>> bufferHandle;
};

void Vapoursynth::GetFrame(const Napi::CallbackInfo &info) {
    int32_t frameNumber = info[0].As<Napi::Number>().Int32Value();
    Napi::Buffer<char> buffer = info[1].As<Napi::Buffer<char>>();
    Napi::Function callback = info[2].As<Napi::Function>();
    FrameWorker *worker = new FrameWorker(this, frameNumber, buffer, callback);
    worker->Queue();
}
