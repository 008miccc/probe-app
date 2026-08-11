// hotpath.cpp
//
// Deliberately minimal set of functions with KNOWN, DISTINCT cost profiles.
// The point is ground truth: you already know which of these should show up
// hot in a sampling profiler, so you can validate that your pipeline
// (simpleperf -> symbolize -> Frida hook) actually finds the right thing,
// rather than just producing plausible-looking output.
//
// All functions are extern "C" and unmangled so Frida can hook them by
// exported name with zero reverse engineering required in this first pass.

#include <jni.h>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <vector>

extern "C" {

// ---------------------------------------------------------------------
// 1) cheap_noop
// Baseline. Should be dominated entirely by JNI crossing overhead, not by
// anything inside the function. In a flame graph this should barely
// register as self-time; almost all cost is the JNI transition around it.
// ---------------------------------------------------------------------
JNIEXPORT jint JNICALL
Java_com_probe_hotpath_NativeBridge_cheapNoop(JNIEnv *, jobject) {
    return 42;
}

// ---------------------------------------------------------------------
// 2) cpu_bound_spin
// Pure integer/float ALU work, no memory traffic to speak of (stays in
// registers), no branches that depend on data. This is your "should be
// cache-miss-clean, should show up hot on cpu-cycles / instructions,
// should NOT show up hot on cache-misses" reference case.
// Cost scales linearly and predictably with `iterations`.
// ---------------------------------------------------------------------
JNIEXPORT jdouble JNICALL
Java_com_probe_hotpath_NativeBridge_cpuBoundSpin(JNIEnv *, jobject, jint iterations) {
    double acc = 0.0;
    for (int i = 0; i < iterations; i++) {
        acc += std::sin(static_cast<double>(i)) * std::cos(static_cast<double>(i));
    }
    return acc;
}

// ---------------------------------------------------------------------
// 3) memory_bound_scan
// Walks a large buffer with a stride designed to defeat cache locality
// (jumping by a prime-ish stride rather than sequential access). This is
// your "should show up hot on cache-misses specifically, cycles-per-byte
// should be much worse than cpu_bound_spin despite doing 'less math'"
// reference case -- the point being to demonstrate that cpu-cycles alone
// doesn't tell you *why* something is slow.
// ---------------------------------------------------------------------
JNIEXPORT jlong JNICALL
Java_com_probe_hotpath_NativeBridge_memoryBoundScan(JNIEnv *, jobject, jint sizeBytes) {
    static std::vector<uint8_t> buffer;
    if (static_cast<jint>(buffer.size()) != sizeBytes) {
        buffer.assign(sizeBytes, 0);
        for (jint i = 0; i < sizeBytes; i++) buffer[i] = static_cast<uint8_t>(i);
    }

    const jint stride = 4099; // prime-ish, larger than a cache line, defeats prefetch
    uint64_t sum = 0;
    jint idx = 0;
    for (jint i = 0; i < sizeBytes; i++) {
        sum += buffer[idx];
        idx = (idx + stride) % sizeBytes;
    }
    return static_cast<jlong>(sum);
}

// ---------------------------------------------------------------------
// 4) branchy_dispatch
// Simulates the shape of an interpreter dispatch loop (a big switch driven
// by essentially unpredictable input), which is structurally the closest
// thing here to what ARMSX3/an emulator's instruction dispatch loop looks
// like. This is your "should show up hot on branch-misses specifically"
// reference case.
// ---------------------------------------------------------------------
JNIEXPORT jint JNICALL
Java_com_probe_hotpath_NativeBridge_branchyDispatch(JNIEnv *, jobject, jint iterations) {
    uint32_t state = 0x9E3779B9u; // arbitrary odd seed
    int acc = 0;
    for (jint i = 0; i < iterations; i++) {
        // cheap xorshift so the branch target is effectively unpredictable
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;

        switch (state & 0x7) {
            case 0: acc += 1; break;
            case 1: acc -= 1; break;
            case 2: acc += 2; break;
            case 3: acc -= 2; break;
            case 4: acc *= 1;  break; // deliberate no-op arm
            case 5: acc ^= 0xF; break;
            case 6: acc += (int)(state & 0xFF); break;
            default: acc -= (int)(state & 0xFF); break;
        }
    }
    return acc;
}

// ---------------------------------------------------------------------
// 5) hle_style_lookup
// Named to mirror an emulator's HLE syscall dispatch: takes an "id",
// looks it up in a small table, calls through a function pointer. This is
// here specifically so you have one function whose *name* signals its role
// the way a real HLE handler's name would, for practicing "grep the
// flame graph output for something that looks like a syscall handler."
// ---------------------------------------------------------------------
static int hle_handler_a(int x) { return x + 1; }
static int hle_handler_b(int x) { return x * 2; }
static int hle_handler_c(int x) { return x ^ 0x5A; }

JNIEXPORT jint JNICALL
Java_com_probe_hotpath_NativeBridge_hleStyleLookup(JNIEnv *, jobject, jint id, jint arg) {
    typedef int (*handler_fn)(int);
    static const handler_fn table[3] = { hle_handler_a, hle_handler_b, hle_handler_c };
    return table[id % 3](arg);
}

// ---------------------------------------------------------------------
// runAll: convenience entry point that calls all of the above in
// proportion, so a single "Run Benchmark" tap produces a flame graph with
// all five shapes represented at once -- useful for a first end-to-end
// sampling pass without needing five separate UI buttons.
// ---------------------------------------------------------------------
JNIEXPORT void JNICALL
Java_com_probe_hotpath_NativeBridge_runAll(JNIEnv *env, jobject thiz) {
    for (int rep = 0; rep < 50; rep++) {
        Java_com_probe_hotpath_NativeBridge_cheapNoop(env, thiz);
        Java_com_probe_hotpath_NativeBridge_cpuBoundSpin(env, thiz, 200000);
        Java_com_probe_hotpath_NativeBridge_memoryBoundScan(env, thiz, 4 * 1024 * 1024);
        Java_com_probe_hotpath_NativeBridge_branchyDispatch(env, thiz, 200000);
        for (int i = 0; i < 1000; i++) {
            Java_com_probe_hotpath_NativeBridge_hleStyleLookup(env, thiz, i, i);
        }
    }
}

} // extern "C"
