## 1.2.4
New features:
* Added audio test app.

## 1.2.3
Fixes:
* Add output gain and output frequency CLI parameters to test tone app.

## 1.2.2
Fixes:
* Reduced GPIO socket write latency.

## 1.2.1
New features:
* Pipewire integration.
* Support alternate shared memory layout.

Fixes:
* Memory allocation problem in latency app.

## 1.2.0
New features:
* Added latency monitor app.
* Refactored test applications.

Fixes:
* Fixed missing `<vector>` include in `raspa_pimpl.h`.
* Fixed `-c` broken option in `load_test` application.

## 1.1.0
New features:
* Added mode switches detection when raspa is build in Debug mode and the RASPA_DEBUG_SIGNAL_ON_MODE_SW flag is passed to raspa_open().
* Reading a new parameter exposed by driver to pin the main RT thread to a specific CPU.

Fixes:
* Disable execution of the unit tests in a cross-compiling environment and just build the binaries to run tests on the target.
* Fixes and changes to the sample converters. We now have linear int-float-int conversion for all formats and good clipping behavior at the cost of a slightly reduced dynamic range on the output for INT32 format.

## 1.0.0
First version with EVL.
