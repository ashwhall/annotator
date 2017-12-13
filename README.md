# Annotator
A simple tool which is used to annotate swimming videos. Allows a user to indicate up to 10 lanes of swimmer coordinates, strokes, and events. Linearly interpolates positions between key-frames for display purposes and outputs an annotation `json` file.

## Usage
The following files are included in the repository in `Annotator/x64/Release` - ensure they are either in the same directory as the executable, or in your system `PATH`.

- libcrypto-1_1-x64.dll
- opencv_ffmpeg320_64.dll
- opencv_world320.dll

Open the application with a video file as a command line argument, or drag and drop a video onto the application shortcut.
The results will be stored in a file of the same name, but with the extension `.json`.

### Notes
This application will only work for x64 Windows.
