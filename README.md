# Clipotype

A multiband saturator / clipper plugin built with JUCE. Clipotype allows you to visually split your audio spectrum into three distinct frequency bands and apply different flavors of saturation and waveshaping to each band independently.

## Control & Parameters
-Intensity knob let you select the type of clipping that happens zero percent intensity means you are soft clipping the sound as you go up to one hundred percent clipping happens harder.  
-Drive knob let you choose how much saturation that effect your sound.  
-Stereo knob let you add width to your sound you can also make your mono by choose it zero percent.  
-Threshold knob let you determine which db and above loudness signals that effect from saturation.  
-Amount let you select how much of the processed sound gets out of the clipper zero percent means sound signal leaves the plugin with no effects on it.  
-With delta knob you can only listen what clipotype adds to your sound and with the bypass knob you can listen your input sound.  
-In the spectrum analyzer you can see your sound spectrum you can split the frequencies up to three band and by the band section thats above you can add different type of saturation to different bands.


## Features
- Up to 3 frequency bands with draggable crossover points
- Per-band Intensity (soft-to-hard waveshaping morph), Drive, and M/S stereo width
- 4x oversampling
- Real-time spectrum analyzer
- VST3 / AU / Standalone

## Building
Requires JUCE 8 and CMake 3.22+.

## License
AGPLv3
