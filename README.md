# WhistleDetector

## Basic Overview
**WhistleDetector** is a console application that publishes MQTT messages whenever a whistle pattern is detected.

## Getting Started

Get started by building the code from source (for *Windows*, *Linux* and *Mac*).

### Prerequisites

To build this application, you will need `Qt5`, `QtMultimedia`, `QMqtt` and `xtensor-fftw`. If you are going to build on Windows, you need to make sure, that your `PATH` variable contains paths to *Qt* and *MinGW* / *MSVC* toolsets (*bin* folder).

Install `QMqtt` module

```bash
git clone https://github.com/emqtt/qmqtt.git
cd qmqtt
qmake
sudo make install # mingw32-make install on Windows
```

Install `miniconda` on ARM

```bash
wget https://github.com/conda-forge/miniforge/releases/latest/download/Miniforge3-Linux-aarch64.sh
bash Miniforge3-Linux-aarch64.sh # reboot after this
```

Install `xtensor-fftw` and its dependencies

```bash
conda install -c conda-forge xtensor-fftw
```

### Building

Clone the GitHub repository
```bash
git clone https://github.com/chrizbee/WhistleDetector
cd WhistleDetector
```

Build it using `qmake` and `make`. Make sure `CONDA_PREFIX` is set before building.
```bash
qmake
make
```
Run the application.
## Deployment

- **Linux** - [linuxdeployqt](https://github.com/probonopd/linuxdeployqt)
- **Windows** - [windeployqt](https://doc.qt.io/qt-5/windows-deployment.html)

## Built With

* [Qt5](https://www.qt.io/) - The UI framework used
* [QMqtt](https://github.com/emqx/qmqtt) - Qt MQTT Client
* [xtensor-fftw](https://github.com/xtensor-stack/xtensor-fftw) - Used for FFT

## Versioning

We use [SemVer](http://semver.org/) for versioning. The current version is stored in [WhistleDetector.pro](WhistleDetector.pro) (`VERSION`).

## Authors

- [chrizbee](https://github.com/chrizbee)

See also the list of [contributors](https://github.com/chrizbee/NewtonFractal/contributors) who participated in this project.

## License

This project is licensed under the Beerware License - see the [LICENSE](LICENSE) file for details.
