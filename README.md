# GUSDIAG

(C) 2025 Eric Voirin

**GUSDIAG** is a tool to test GUS classic sound cards.

It's very basic and doesn't have many features. I wrote this to diagnose and repair a friend's GUS card and the official tools aren't very helpful at all.

## Usage

Without any parameters it scans ports 0x220, 0x240, 0x260 and 0x280 for GUS cards and tests the DRAM on any cards it finds (and reports the size).

* `-v`: Extra verbose output
* `-a:<addr>`: Probe specifically only this address
* `-i`: Ignore probe/reset errors and test memory anyway (you should only use this together with -a for obvious reasons.)

## Building

* Download and install the OpenWatcom v2 toolchain
* Clone the repository directory
* Type `wmake`
* That's it!

# License

[Creative Commons Attribution-NonCommercial-ShareAlike 4.0 (CC BY-NC-SA 4.0)](https://creativecommons.org/licenses/by-nc/4.0/deed.en)