# Autocord for Android

Autocord is an experimental Android app that automatically triggers audio recording based on environmental sound levels and frequencies. Originally designed to capture bird calls (e.g., dawn chorus) while filtering out noise like passing cars.

📷 It also included phone call recording when that was still permitted by Android.

This open source release is provided as-is without support or warranties.
Developers are welcome to build on it, adapt it, or integrate it into their own projects.
This project is released as-is and no longer actively maintained, but it's fully functional and open to anyone who wants to build on it.
Feel free to fork it, modernize it, or adapt it for your own purposes.
---

## Features

- Noise and frequency triggered recording with smart thresholds
- Adjustable sensitivity and trigger delay
- Experimental support for phone call recording (older Android only)
- Native audio processing via C++

## Status

This project is no longer in active development but is provided as a reference.

✅ Last known good build: [Apr. 27, 2021]  
🛠️ Requires: Android Studio (compileSdkVersion 30) + NDK (tested on [21.0.6113669])  
📱 Best run on real hardware (emulator has no mic input)

---

## Building

To build:

1. Clone this repo
2. Open in Android Studio
3. Install NDK if prompted
4. Build > Rebuild Project

---

## License

This project is licensed under the [Apache License 2.0](LICENSE).

### Patent Notice

Use of this source code does not imply a license to the patent beyond what is granted by the open source license.

---

## Contact

George W. Taylor  
🌐 https://tropicalcoder.com  
📧 gtaylor@tropicalcoder.com



This project is open source under the [MIT License](LICENSE).

© 2025 George W. Taylor. All rights reserved.