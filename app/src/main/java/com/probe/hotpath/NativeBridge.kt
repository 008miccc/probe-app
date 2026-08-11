package com.probe.hotpath

object NativeBridge {
    init {
        System.loadLibrary("hotpath")
    }

    external fun cheapNoop(): Int
    external fun cpuBoundSpin(iterations: Int): Double
    external fun memoryBoundScan(sizeBytes: Int): Long
    external fun branchyDispatch(iterations: Int): Int
    external fun hleStyleLookup(id: Int, arg: Int): Int
    external fun runAll()
}
