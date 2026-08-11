package com.probe.hotpath

import android.os.Bundle
import android.widget.Button
import android.widget.TextView
import android.widget.LinearLayout
import androidx.appcompat.app.AppCompatActivity
import kotlin.concurrent.thread

class MainActivity : AppCompatActivity() {

    @Volatile private var running = false

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val status = TextView(this).apply {
            text = "Idle. Tap Start, then attach simpleperf/Frida to this process."
            setPadding(32, 64, 32, 32)
        }

        val startButton = Button(this).apply {
            text = "Start continuous benchmark"
        }

        val stopButton = Button(this).apply {
            text = "Stop"
        }

        startButton.setOnClickListener {
            if (running) return@setOnClickListener
            running = true
            status.text = "Running (pid visible via adb shell pidof ${packageName})..."
            thread(start = true, isDaemon = true) {
                // Loop indefinitely so there's a long, stable window to
                // attach simpleperf/Frida against -- a single short burst
                // is too small a sampling window to be useful.
                while (running) {
                    NativeBridge.runAll()
                }
            }
        }

        stopButton.setOnClickListener {
            running = false
            status.text = "Stopped."
        }

        val layout = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            addView(status)
            addView(startButton)
            addView(stopButton)
        }

        setContentView(layout)
    }
}
