// This file will include the power supply riser module firmware
// which can be RAM-loaded using ISP without any flash modifications on the module.
// This means that the alternate firmware can be installed with a simple USB
// mass-storage operation.
// The original EA firmware should be backed up (copy the firmware.bin file)
// before the alternate firmware is flashed!

// Build the ps2k-front project which will trigger building the ps2k-riser project
// creating the binary which is included below.
// The ps2k-front project will create the firmware.bin to be copied to the power supply
// over USB after powering on with ch1 preset and on/off buttons pressed.

    .section ".text"
    .global modulefw
    .global modulefwsize
    .type modulefw, "object"
modulefw:
    .incbin "../../ps2k-riser/Debug/ps2k-riser.bin"
modulefwsize:
    .word .-modulefw
