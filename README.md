# WhistleDetector

## Basic Overview
**WhistleDetector** is a console application that publishes MQTT messages whenever a whistle pattern is detected.

## Getting Started

Get started by building the code from source (for *Windows*, *Linux* and *Mac*).

### Prerequisites

To build this application, you will need `Qt5`, `QtMultimedia`, `Qt5Network` `xtensor-fftw` and `cmake`(see links below). If you are going to build on Windows, you need to make sure, that your `PATH` variable contains paths to *Qt* and *MinGW* / *MSVC* toolsets (*bin* folder). For a detailed guide on how to install these on Ubuntu (ARM) see [setup_arm.md](setup_arm.md).

## Building

Clone the GitHub repository.
```bash
git clone https://github.com/chrizbee/WhistleDetector
cd WhistleDetector
```

Build it using `cmake`. Make sure `CONDA_PREFIX` is set before building.

```bash
mkdir build
cd build
cmake ..
cmake --build . [-- -j4]
```
Edit `config.ini` to use your own configuration and MQTT topics.

```bash
nano WhistleDetector/config.ini
```

Run the application.

## Deployment

- **Linux** - [linuxdeployqt](https://github.com/probonopd/linuxdeployqt)
- **Windows** - [windeployqt](https://doc.qt.io/qt-5/windows-deployment.html)

## Built With

* [Qt5](https://www.qt.io/) - The UI framework used
* [xtensor-fftw](https://github.com/xtensor-stack/xtensor-fftw) - Used for FFT
* [CMake](https://cmake.org/) - Build the application

## Versioning

We use [SemVer](http://semver.org/) for versioning. The current version is stored in [WhistleDetector.pro](WhistleDetector.pro) (`VERSION`).

## Authors

- [chrizbee](https://github.com/chrizbee)

See also the list of [contributors](https://github.com/chrizbee/NewtonFractal/contributors) who participated in this project.

## License

This project is licensed under the Beerware License - see the [LICENSE](LICENSE) file for details.
