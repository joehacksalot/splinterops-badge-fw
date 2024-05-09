# 2024-badge-app

Badge 2nd stage bootloader and application project

[[_TOC_]]

## Getting started

Do not forget to download the submodule projects
```bash
git submodule update --init --recursive
```

## Description

A description goes here - mostly of why submodules are being used :P

## Mantinance

### Flashing a badge

```bash
idf.py -p $(ls /dev/cu.usbserial-*) erase-flash
```

```bash
idf.py -p $(ls /dev/cu.usbserial-*) flash
```

<details>
<summary>All-in-one</summary>

```bash
idf.py -p $(ls /dev/cu.usbserial-*) erase-flash && idf.py -p $(ls /dev/cu.usbserial-*) flash
```

</details>

### Monitoring code execution

```bash
idf.py -p $(ls /dev/cu.usbserial-*) monitor
```