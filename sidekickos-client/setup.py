#!/usr/bin/env python3

from setuptools import setup, find_packages

with open("../docs/python-library.md", "r", encoding="utf-8") as fh:
    long_description = fh.read()

with open("requirements.txt", "r", encoding="utf-8") as fh:
    requirements = [line.strip() for line in fh if line.strip() and not line.startswith("#")]

setup(
    name="sidekickos",
    version="1.0.0",
    author="SidekickOS Team",
    description="Python library for ESP32S3 BLE camera interface",
    long_description=long_description,
    long_description_content_type="text/markdown",
    url="https://github.com/henrywarren/SidekickOS",
    packages=find_packages(),
    classifiers=[
        "Development Status :: 4 - Beta",
        "Intended Audience :: Developers",
        "License :: OSI Approved :: MIT License",
        "Operating System :: OS Independent",
        "Programming Language :: Python :: 3",
        "Programming Language :: Python :: 3.7",
        "Programming Language :: Python :: 3.8",
        "Programming Language :: Python :: 3.9",
        "Programming Language :: Python :: 3.10",
        "Programming Language :: Python :: 3.11",
        "Topic :: Multimedia :: Graphics :: Capture :: Digital Camera",
        "Topic :: Software Development :: Libraries :: Python Modules",
    ],
    python_requires=">=3.7",
    install_requires=requirements,
    keywords="esp32 camera ble bluetooth image capture opencv",
    project_urls={
        "Bug Reports": "https://github.com/henrywarren/SidekickOS/issues",
        "Source": "https://github.com/henrywarren/SidekickOS",
        "Documentation": "https://github.com/henrywarren/SidekickOS/blob/main/README.md",
    },
) 