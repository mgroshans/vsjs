#include <node.h>
#include "vsjs.h"

using namespace v8;

void InitAll(Handle<Object> exports) {
    Vapoursynth::Init(exports);
}

NODE_MODULE(vsjs, InitAll)