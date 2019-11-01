NanoVNA-Q - Stable firmware for NanoVNA
==========================================================

[![GitHub release](https://img.shields.io/github/v/release/qrp73/NanoVNA-Q.svg?style=flat)][release]

[release]: https://github.com/qrp73/NanoVNA-Q/releases

<div align="center">
<img src="https://user-images.githubusercontent.com/46676744/67703264-d3418d80-f9bb-11e9-99ff-ffb23ba3f3fd.png" width="480px">
</div>

# About

NanoVNA-Q is firmware for NanoVNA vector network analyzer.

Original NanoVNA firmware and hardware was developed by @edy555 and it's source code can be found here: https://github.com/ttrftech/NanoVNA

Later, @hugen79 introduced a new PCB (NanoVNA-H) and improvements for firmware and device become very popular. @hugen79 project can be found here: https://github.com/hugen79/NanoVNA-H

NanoVNA-Q is based on @edy555 code, includes improvements from @hugen79 and is targeted for NanoVNA-H hardware.

The main goal of this project is to fix bugs, improve stability, measurement quality and usability.


The main differences with original firmware:
- added impedance label for current marker
- improved noise floor, imbalance gain, measurement quality and data transfer to PC
- added scanraw command (allows to read raw gamma data for unlimited point count with no calibration apply)
- added color command (allows to customize trace colors)
- fixed frequency rounding issues
- fixed multithreading issues
- added si5351 PLL lock hardware check
- fixed couple of bugs


## Reference

* [NanoVNA-H Schematic](/doc/NanoVNA-H_V3.0_Jul-11-19.pdf)
* [PCB Photo](/doc/nanovna-pcb-photo.jpg)
* [Block Diagram](/doc/nanovna-blockdiagram.png)
* [NanoVNA-H](https://github.com/hugen79/NanoVNA-H)

### Contributors

* [@edy555](https://github.com/edy555)
* [@hugen79](https://github.com/hugen79)
* [@cho45](https://github.com/cho45)
* [@qrp73](https://github.com/qrp73)
