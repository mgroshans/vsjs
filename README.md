#vsjs
VapourSynth for JavaScript.

##Documentation
### Vapoursynth Object
#### Constructor
This module allows you to create `Vapoursynth` objects with the following
syntax:
```javascript
let clip = vsjs.Vapoursynth(scriptContents, [scriptFilename]);
```
`scriptContents` is the actual contents of the script (as a buffer) which
should be evaluated.

`scriptFilename` is the path of the script being evaluated. This is optional.
See
[the vapoursynth C documentation](http://vapoursynth.com/doc/api/vsscript.h.html#vsscript-evaluatescript)
for more information on vapoursynth's behavior if no filename is specified.

#### getInfo()
```javascript
clip.getInfo();
```
Which returns some useful information about the clip:
```json
{
  "height": 480,
  "width": 640,
  "numFrames": 1000,
  "fps": {
    "numerator": 24000,
    "denominator": 1001
  },
  "frameSize": 460800
}
```
`frameSize` indicates how large the buffer must be to fetch frames from this
clip.

#### getFrame()
```javascript
clip.getFrame(frameNumber, frameBuffer, callback);
```
`frameNumber` is the desired frame.

`frameBuffer` is the destination buffer. If no error occurs it will be populated
with the frame data.

`callback` will be called when the frame data has been fetched with the
following arguments:
```javascript
callback(error, frameNumber, frameBuffer);
```
`error` is any exception encountered while getting the frame. If no exception
was encountered it will be null.

`frameNumber` is the number of the fetched frame.

`frameBuffer` is the same buffer which was originally provided, but now with
populated with frame data.

##Example
```javascript
const fs = require('fs');
const vsjs = require('vsjs');

const pathToScript = 'C:\\VapourSynth\\example.vpy';
const clip = vsjs.Vapoursynth(fs.readFileSync(pathToScript), pathToScript);

console.log(clip.getInfo());

clip.getFrame(0, Buffer.alloc(clip.getInfo().frameSize), (err, frameNumber, buffer) => {
    if (err) {
        console.log(err);
    } else {
        console.log(`buffer now contains frame ${frameNumber}`);
    }
});
```

##Build
Install our dependencies:
```
> npm install
```
Then we go through the normal node-gyp process:
```
> node-gyp configure [--VS_SDK="C:/Vapoursynth/Install/sdk"]
> node-gyp build
```
To be honest I'm not sure if I've followed all the best practices for gyp
builds or if the build will work on non-windows systems. If you have trouble
building please try editing the binding.gyp or open an issue. 
