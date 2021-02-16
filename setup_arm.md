# Setup for ARM

The following guide is on how to setup WhistleDetector on a fresh Ubuntu installation for ARM (Odroid C2).

## Install OS

Download the latest official release (minimal version) from [here](https://wiki.odroid.com/odroid-c2/os_images/ubuntu/ubuntu). You can also try Armbian or any other image, but this guide focuses on the official one.

Flash the image using e.g. [Etcher](https://www.balena.io/etcher/).

Login locally or via SSH and update the system. Credentials are `root` / `odroid`.

```bash
sudo apt update
sudo apt upgrade
```

Add your user if it doesn't exist already.

```bash
sudo adduser chrizbee sudo
```

## Install dependencies

To build this application, you will need `Qt5`, `QtMultimedia`, `QMqtt` and `xtensor-fftw`.

I wasn't able to use Alsa with the official Qt-Mulimedia packages since they were built with only one audio backend - which happens to be `pulseaudio`.

```bash
sudo apt install qt5-default qtmultimedia5-dev libqt5multimedia5-plugins pulseaudio git libfftw3-dev
```

Enable `pulseaudio` service and set default source.

```bash
systemctl --user enable pulseaudio
sudo usermod -aG audio chrizbee
pacmd list-sources # Remember the index
pacmd set-default-source <index>
```

Install `QMqtt` module.

```bash
git clone https://github.com/emqtt/qmqtt.git
cd qmqtt
qmake
sudo make install
```

The best way to install `xtensor-fftw` and its dependencies is by using `conda`. You don't need to automatically initialize by running `conda init`. Instead just export `CONDA_PREFIX` on startup.

```bash
wget https://github.com/conda-forge/miniforge/releases/latest/download/Miniforge3-Linux-aarch64.sh
bash Miniforge3-Linux-aarch64.sh
```

Append the following line to `~/.bashrc`.

```bash
# Miniconda Path
export CONDA_PREFIX=/home/chrizbee/miniforge3
export PATH=$PATH:$CONDA_PREFIX/bin
```

Reboot and install `xtensor-fftw` with its dependencies.

```bash
conda install -c conda-forge xtensor-fftw
```

## Install WhistleDetector

Clone the GitHub repository.

```bash
git clone https://github.com/chrizbee/WhistleDetector
cd WhistleDetector
```

Edit `config.h` to use your own configuration and MQTT topics.

```bash
nano src/config.h
```

Build it using `qmake` and `make`. Make sure `CONDA_PREFIX` is set before building.

```bash
qmake
make
```

Create a systemd service.

```bash
sudo nano /etc/systemd/user/whistledetector.service
```

```ini
[Unit]
Description=WhistleDetector
After=network.target
StartLimitIntervalSec=0

[Service]
Type=simple
Restart=always
RestartSec=1
ExecStart=/home/chrizbee/WhistleDetector/build/WhistleDetector

[Install]
WantedBy=default.target
```

Enable the service and reboot.

```bash
systemctl --user enable whistledetector
sudo reboot
```