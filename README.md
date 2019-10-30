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
Later, @hugen79 introduced a new PCB (NanoVNA-H) and device become very polular.
This project is based on @edy555 code and targeted for NanoVNA-H hardware.
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

* [Schematics](/doc/nanovna-sch.pdf)
* [PCB Photo](/doc/nanovna-pcb-photo.jpg)
* [Block Diagram](/doc/nanovna-blockdiagram.png)
* [NanoVNA-H](https://github.com/hugen79/NanoVNA-H)

## Credit

* [@edy555](https://github.com/edy555)
* [@hugen79](https://github.com/hugen79)
* [@cho45](https://github.com/cho45)

### Contributors

* [@qrp73](https://github.com/qrp73)
