# Usage

## Statistics Extractor
[![Tutorial](https://raw.githubusercontent.com/NiHoel/Anno1404Calculator/master/CalculatorExtractionScreenshot.png)](https://youtu.be/k4WmgEIkp4s)

- [one-time setup] download, install (and reboot your computer afterwards): [vc_redist](https://support.microsoft.com/en-gb/help/2977003/the-latest-supported-visual-c-downloads)
- run the Server.exe from the [zip archive](https://github.com/NiHoel/Anno1404UXEnhancer/releases/latest/) which should open a command window and might require administrator rights
- run [Anno1404Calculator](https://github.com/NiHoel/Anno1404Calculator/releases/latest/) from local file (if not done already)
- run Anno 1404
- open the population overview to permanently update the number of residents or open the reeve's book
- [Tutorial](https://youtu.be/k4WmgEIkp4s)


# Troubleshooting
In case **vcruntime140_1.dll** is missing, download the vc_redist from [https://support.microsoft.com/en-gb/help/2977003/the-latest-supported-visual-c-downloads](https://support.microsoft.com/en-gb/help/2977003/the-latest-supported-visual-c-downloads)

In case nothing happens, make sure:
- the ingame text language and the language of the calculator are identical
- the values to be read are not covered by something else (overlay, external program, etc.; basically things you would see when running a screen capturing program)
- The window mode of the game is set to 'windowed full screen'
- Disable XBOX DVR 
- The correct island is selected in the statistics screen
- The island has a long (> 8 letters) name composed from standard letters (A-Za-z) and with other islands as few characters in common as possible
  - NEGATIVE example: "Múa-1" and "Múa-2": Too short, non standard letter ú and both names only differ in one letter
- In the center of the statistics menu the selected entry is fully visible

- If the game window is not found (e.g. you stream it from a cloud gaming platform), then you can specify the title of the window manually. Enter ".\UXEnhancer.exe -w " (repectively ".\Server.exe -w ") followed by the title of the window in quotation marks. The string you specify is interpreted as a regular expression. This means that '.' is a wildcard and "()[]*\" are reserved characters.

**If you encounter any bug, feel free to contact me (e.g. open an issue) and if possible perform the following steps**
- Open the program in the console with verbose option:
- Shift + right click on folder containing the exe file -> Open PowerShell Window -> Enter ".\Server.exe -v" (without the quotes)
- Configure and open the store / Open the statistics screen or where the bug occurred
- Close the program without closing any dialog in Anno
- Send me the console output + debug_images


# Use prebuild binaries
- download the latest release from https://github.com/NiHoel/Anno1404UXEnhancer/releases/latest/
- extract the archive to any location you desire

# Build it yourself 
## Requirements
- Git-installation (e.g. https://git-scm.com/download/win)
- visual studio 2017 or higher (https://visualstudio.microsoft.com/de/vs/)
	
## Build instructions
- git clone https://github.com/NiHoel/Anno1404UXEnhancer.git
- cd Anno1404UXEnhancer
- SETUP.bat
- vcpkg install boost-property-tree:x64-windows (takes circa 2 minutes)
- vcpkg install tesseract:x64-windows (takes circa 17 minutes)
- vcpkg install cpprestsdk[core]:x64-windows (takes circa 10 minutes)
- vcpkg install opencv4[png]:x64-windows opencv4[jpeg]:x64-windows (takes circa 7 minutes)
- get the files for assets/tessdata from the zip archive from https://github.com/NiHoel/Anno1800UXEnhancer/releases/latest/
- <build cpp/visual studio/UXEnhancer.sln>
			
## Troubleshooting 
- copy, move, rename errors during installation: make sure that vcpkg resides on a short path (e.g. accass the folder via a drive letter)
- vcpkg error "Please install the English language pack. Could not locate a complete toolset."
-> go to visual studio installer -> visual studio communitiy -> change -> language package
-> select english -> click change (bottom right)
- if visual studio ist not installed on C:/Program Files x86 openGL (as a part of openCV) might fail to build
possible fix: https://github.com/Microsoft/vcpkg/issues/4377 (untested)	
or reinstall windows kit on C:/Program Files x86 (can be more tricky than you think)
	
