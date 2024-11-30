// index.h

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Device Dashboard</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            background-color: #121212;
            color: #E0E0E0;
            margin: 0;
            padding: 0;
        }

        .container {
            max-width: 960px;
            margin: 20px auto;
            background-color: #1E1E1E;
            border-radius: 8px;
            padding: 20px;
            box-shadow: 0 4px 8px rgba(0, 0, 0, 0.2);
        }

        h1 {
            text-align: center;
            color: #ffffff;
        }

        .info {
            margin: 10px 0;
            padding: 10px;
            background-color: #333;
            border-left: 6px solid #2196F3;
        }

        .form-group {
            margin-bottom: 15px;
        }

        label {
            display: block;
            margin-bottom: 5px;
            color: #B3B3B3;
        }

        input, select, button, .slider {
            width: 100%;
            padding: 8px;
            margin-top: 5px;
            border-radius: 4px;
            border: 1px solid #444;
            background-color: #333;
            color: #E0E0E0;
        }

        input[type="checkbox"], input[type="radio"] {
            width: auto;
            margin-right: 10px;
        }

        table {
            width: 100%;
            border-collapse: collapse;
            margin-bottom: 20px;
        }

        table, th, td {
            border: 1px solid #444;
        }

        th, td {
            padding: 10px;
            text-align: left;
            background-color: #333;
        }

        th {
            background-color: #2196F3;
            color: white;
        }

        .slider {
            -webkit-appearance: none;
            width: 100%;
            height: 12px;
            border-radius: 6px;
            background: #8a8a8a;
            outline: none;
        }

        .slider::-webkit-slider-thumb {
            -webkit-appearance: none;
            appearance: none;
            width: 20px;
            height: 20px;
            border-radius: 50%;
            background: #2196F3;
            cursor: pointer;
        }

        .slider::-moz-range-thumb {
            width: 20px;
            height: 20px;
            border-radius: 50%;
            background: #2196F3;
            cursor: pointer;
        }

        .section-title {
            margin-top: 20px;
            font-size: 1.2em;
            border-bottom: 2px solid #333;
            padding-bottom: 5px;
            color: #ffffff;
        }

        .greyed-out {
            background-color: #333;
            pointer-events: none;
            opacity: 0.5;
        }

        .status-section {
            display: flex;
            justify-content: space-between;
        }

        .slider-value {
            float: right;
            color: #ffffff;
            margin-left: 10px;
        }

        a {
            color: #2196F3;
            text-decoration: none;
        }

        a:hover {
            text-decoration: underline;
        }

    </style>
</head>
<body>

    <div class="container">
        <h1>Device Dashboard</h1>

        <!-- Status Section -->
        <div class="status-section">
            <div class="info">
                <strong>Device Name: </strong>
                <span id="deviceName" onclick="editDeviceName()">Loading...</span>
                <input type="text" id="deviceNameInput" style="display: none;" onblur="saveDeviceName()" onkeydown="checkEnter(event)">
            </div>
            <div class="info">
                <strong>IP Address: </strong><a href="#" id="deviceIP" target="_blank">Loading...</a>
            </div>
        </div>

        <!-- Device Info Table -->
        <div class="section-title">Device Status</div>
        <table>
            <tr><th>Memory Usage</th><td id="memoryUsage">Loading...</td></tr>
            <tr><th>CPU Usage</th><td id="cpuUsage">Loading...</td></tr>
            <tr><th>Uptime</th><td id="uptime">Loading...</td></tr>
            <tr><th>Battery %</th><td id="batteryPercentage">Loading...</td></tr>
        </table>

        <!-- Theatre Chat Section -->
        <div class="section-title">OSC Sending</div>
        <div class="form-group">
            <label>
                <input type="checkbox" id="msgSendToggle"> OSC Sending On/Off
            </label>
        </div>
        <div id="theatreChatSection">
            <div class="form-group">
                <label for="chatPort">Port:</label>
                <input type="number" id="chatPort" placeholder="Enter Port">
            </div>
            <div class="form-group">
                <label for="chatChannel">Channel:</label>
                <input type="text" id="chatChannel" placeholder="Enter Channel">
            </div>
            <div class="form-group">
                <label for="chatName">Name:</label>
                <input type="text" id="chatName" placeholder="Enter Chat Name" readonly>
            </div>
            <div class="form-group" style="display: flex; align-items: center;">
                <label for="chatMessage" style="flex: 1;">Message:</label>
                <input type="text" id="chatMessage" placeholder="Enter Message" style="flex: 3;">
                <button id="sendTestOsc" style="flex: 1; margin-left: 10px;">Send Test OSC</button>
            </div>
        </div>
        <!-- OSC Section -->
        <div class="section-title">OSC Configuration</div>
        <div class="form-group">
            <label>
                <input type="checkbox" id="oscToggle"> OSC Receive On/Off
            </label>
        </div>
        <div id="oscSection">
            <div class="form-group">
                <label for="oscPort">OSC Receive Port (Requires Restart):</label>
                <input type="number" id="oscPort" placeholder="Enter OSC Port">
            </div>
        </div>
        <!-- Camera Settings Section -->
        <div class="section-title">Camera Settings</div>
        <div class="form-group">
            <label for="brightness">Brightness <span class="slider-value" id="brightnessValue">0</span></label>
            <input type="range" id="brightness" min="-2" max="2" value="0" class="slider">
        </div>
        <div class="form-group">
            <label for="contrast">Contrast <span class="slider-value" id="contrastValue">0</span></label>
            <input type="range" id="contrast" min="-2" max="2" value="0" class="slider">
        </div>
        <div class="form-group">
            <label for="saturation">Saturation <span class="slider-value" id="saturationValue">0</span></label>
            <input type="range" id="saturation" min="-2" max="2" value="0" class="slider">
        </div>
        <div class="form-group">
            <label for="specialEffect">Special Effect:</label>
            <select id="specialEffect">
                <option value="0">No Effect</option>
                <option value="1">Negative</option>
                <option value="2">Grayscale</option>
                <option value="3">Red Tint</option>
                <option value="4">Green Tint</option>
                <option value="5">Blue Tint</option>
                <option value="6">Sepia</option>
            </select>
        </div>
        <div class="form-group">
            <label><input type="checkbox" id="whitebal"> White Balance</label>
        </div>
        <div class="form-group">
            <label><input type="checkbox" id="awbGain"> AWB Gain</label>
        </div>
        <div class="form-group">
            <label for="wbMode">White Balance Mode:</label>
            <select id="wbMode">
                <option value="0">Auto</option>
                <option value="1">Sunny</option>
                <option value="2">Cloudy</option>
                <option value="3">Office</option>
                <option value="4">Home</option>
            </select>
        </div>
        <div class="form-group">
            <label for="quality">Quality <span class="slider-value" id="qualityValue">6</span></label>
            <input type="range" id="quality" min="5" max="20" value="6" class="slider">
        </div>
        <div class="form-group">
            <label for="frameRate">Frame Rate <span class="slider-value" id="frameRateValue">30</span> FPS</label>
            <input type="range" id="frameRate" min="1" max="120" value="30" class="slider">
        </div>
        <div class="form-group">
            <label><input type="checkbox" id="flip"> Flip</label>
        </div>
        <div class="form-group">
            <label><input type="checkbox" id="mirror"> Mirror</label>
        </div>

        <!-- IR Settings Section -->
        <div class="section-title">IR Settings</div>
        <div class="form-group">
            <label><input type="checkbox" id="irOn"> IR On/Off</label>
        </div>

        <!-- Sound Settings Section -->
        <div class="section-title">Detection Settings</div>
        <div class="form-group">
            <label><input type="checkbox" id="soundDetectionToggle"> Enable/Disable Sound Detection</label>
        </div>
        <div class="form-group">
            <label><input type="checkbox" id="PIRDetectionToggle"> Enable/Disable PIR Detection</label>
        </div>

        <!-- Sound Threshold Input -->
        <div class="form-group">
            <label for="soundThreshold">Sound Threshold:</label>
            <input type="number" id="soundThreshold" placeholder="Enter Sound Threshold">
        </div>

        <!-- Sound Debounce Delay Input -->
        <div class="form-group">
            <label for="soundDebounce">Sound Debounce Delay (ms):</label>
            <input type="number" id="soundDebounce" placeholder="Enter Sound Debounce Delay">
        </div>

        <!-- PIR Debounce Delay Input -->
        <div class="form-group">
            <label for="pirDebounce">PIR Debounce Delay (ms):</label>
            <input type="number" id="pirDebounce" placeholder="Enter PIR Debounce Delay">
        </div>

        <!-- Device Orientation Section -->
        <div class="section-title">Device Orientation (Requires reboot to fully apply)</div>
        <div class="form-group">
            <label><input type="radio" name="orientation" value="0"> 0°</label>
            <label><input type="radio" name="orientation" value="90"> 90°</label>
            <label><input type="radio" name="orientation" value="180"> 180°</label>
            <label><input type="radio" name="orientation" value="270"> 270°</label>
        </div>
        <!-- LED Control Section -->
        <div class="section-title">LED Control</div>

        <div class="form-group">
            <label><input type="checkbox" id="ledToggle"> Enable/Disable LEDs</label>
        </div>
        <div class="form-group">
            <label for="ledBrightness">LED Brightness<span class="slider-value" id="ledBrightnessValue">255</span></label>
            <input type="range" id="ledBrightness" min="0" max="255" value="255" class="slider">
        </div>
        <!-- Color Selection for Mic On -->
        <div class="form-group">
            <label for="micOnColor">Select Color for Mic On:</label>
            <input type="color" id="micOnColor" value="#00FF00">  <!-- Default Green -->
        </div>

        <!-- Color Selection for Mic Off -->
        <div class="form-group">
            <label for="micOffColor">Select Color for Mic Off:</label>
            <input type="color" id="micOffColor" value="#FF0000">  <!-- Default Red -->
        </div>

        <!-- Color Selection for Mic Ready -->
        <div class="form-group">
            <label for="micReadyColor">Select Color for Mic Ready:</label>
            <input type="color" id="micReadyColor" value="#0000FF">  <!-- Default Blue -->
        </div>

        <!-- Debug Mode Section -->
        <div class="section-title">Debug Mode</div>
        <div class="form-group">
            <label><input type="checkbox" id="debugModeToggle"> Debug Mode On/Off</label>
        </div>
        <!-- Restart Button -->
        <div class="form-group">
            <button id="restartDevice" style="width: 100%;">Restart Device</button>
        </div>

        <!-- Backstage Mode -->
        <div class="form-group">
        </div>
    </div>

<script>
    // JavaScript to update the slider values in real time
    const sliders = document.querySelectorAll('input[type="range"]');
    sliders.forEach(slider => {
        const valueDisplay = document.getElementById(slider.id + 'Value');
        slider.addEventListener('input', function () {
            valueDisplay.textContent = slider.value;
        });
    });

    // JavaScript for Debug Mode Toggle
    document.getElementById('debugModeToggle').addEventListener('change', function () {
        const debugMode = this.checked ? '1' : '0';
        const xhr = new XMLHttpRequest();
        xhr.open('POST', '/setDebugMode', true);
        xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
        xhr.send('mode=' + debugMode);
    });

    // JavaScript for soundDetectionToggle Toggle
    document.getElementById('soundDetectionToggle').addEventListener('change', function () {
        const soundDetectionToggle = this.checked ? '1' : '0';
        const xhr = new XMLHttpRequest();
        xhr.open('POST', '/soundDetectionToggle', true);
        xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
        xhr.send('soundDetectionToggle=' + soundDetectionToggle);
    });

    // JavaScript for PIRDetectionToggle Toggle
    document.getElementById('PIRDetectionToggle').addEventListener('change', function () {
        const PIRDetectionToggle = this.checked ? '1' : '0';
        const xhr = new XMLHttpRequest();
        xhr.open('POST', '/PIRDetectionToggle', true);
        xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
        xhr.send('PIRDetectionToggle=' + PIRDetectionToggle);
    });

    // JavaScript for IR Toggle
    document.getElementById('irOn').addEventListener('change', function () {
        const irEnabled = this.checked ? '1' : '0';
        const xhr = new XMLHttpRequest();
        xhr.open('POST', '/setIr', true);
        xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
        xhr.send('ir=' + irEnabled);
    });

    // JavaScript for Flip Setting
    document.getElementById('flip').addEventListener('change', function () {
        const flip = this.checked ? '1' : '0';
        const xhr = new XMLHttpRequest();
        xhr.open('POST', '/setCameraFlip', true);
        xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
        xhr.send('flip=' + flip);
    });

    // JavaScript for Restart Button
    document.getElementById('restartDevice').addEventListener('click', function () {
        if (confirm("Are you sure you want to restart the device?")) {
            const xhr = new XMLHttpRequest();
            xhr.open('POST', '/restartDevice', true);
            xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
            xhr.send();
            alert("Device is restarting...");
        }
    });


    // JavaScript for Mirror Setting
    document.getElementById('mirror').addEventListener('change', function () {
        const mirror = this.checked ? '1' : '0';
        const xhr = new XMLHttpRequest();
        xhr.open('POST', '/setCameraMirror', true);
        xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
        xhr.send('mirror=' + mirror);
    });

    // JavaScript for Frame Rate Slider
    document.getElementById('frameRate').addEventListener('input', function () {
        const frameRate = this.value;
        const xhr = new XMLHttpRequest();
        xhr.open('POST', '/setFrameRate', true);
        xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
        xhr.send('frameRate=' + frameRate);

        // Update the displayed frame rate value
        document.getElementById('frameRateValue').textContent = frameRate;
    });


    // JavaScript for Brightness Slider
    document.getElementById('brightness').addEventListener('input', function () {
        const brightness = parseInt(this.value, 10);
        const xhr = new XMLHttpRequest();
        xhr.open('POST', '/setCameraBrightness', true);
        xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
        xhr.send('brightness=' + brightness);
    });

    // JavaScript for Saturation Slider
    document.getElementById('saturation').addEventListener('input', function () {
        const saturation = this.value;
        const xhr = new XMLHttpRequest();
        xhr.open('POST', '/setCameraSaturation', true);
        xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
        xhr.send('saturation=' + saturation);
    });

    // JavaScript for Special Effect Dropdown
    document.getElementById('specialEffect').addEventListener('change', function () {
        const specialEffect = this.value;
        const xhr = new XMLHttpRequest();
        xhr.open('POST', '/setCameraSpecialEffect', true);
        xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
        xhr.send('specialEffect=' + specialEffect);
    });

    // JavaScript for White Balance Toggle
    document.getElementById('whitebal').addEventListener('change', function () {
        const whitebal = this.checked ? '1' : '0';
        const xhr = new XMLHttpRequest();
        xhr.open('POST', '/setCameraWhiteBalance', true);
        xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
        xhr.send('whitebal=' + whitebal);
    });

    // JavaScript for AWB Gain Toggle
    document.getElementById('awbGain').addEventListener('change', function () {
        const awbGain = this.checked ? '1' : '0';
        const xhr = new XMLHttpRequest();
        xhr.open('POST', '/setCameraAwbGain', true);
        xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
        xhr.send('awbGain=' + awbGain);
    });

    // JavaScript for White Balance Mode Dropdown
    document.getElementById('wbMode').addEventListener('change', function () {
        const wbMode = this.value;
        const xhr = new XMLHttpRequest();
        xhr.open('POST', '/setCameraWbMode', true);
        xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
        xhr.send('wbMode=' + wbMode);
    });

    // JavaScript for Contrast Slider
    document.getElementById('contrast').addEventListener('input', function () {
        const contrast = this.value;
        const xhr = new XMLHttpRequest();
        xhr.open('POST', '/setCameraContrast', true);
        xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
        xhr.send('contrast=' + contrast);
    });

    // JavaScript for Quality Slider
    document.getElementById('quality').addEventListener('input', function () {
        const quality = this.value;
        const xhr = new XMLHttpRequest();
        xhr.open('POST', '/setCameraQuality', true);
        xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
        xhr.send('quality=' + quality);
    });

    // JavaScript for LED Brightness Slider
    document.getElementById('ledBrightness').addEventListener('input', function () {
        const ledBrightness = this.value;
        const xhr = new XMLHttpRequest();
        xhr.open('POST', '/setLedBrightness', true);
        xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
        xhr.send('ledBrightness=' + ledBrightness);
    });

    // Add event listeners for Sound Threshold, Sound Debounce Delay, and PIR Debounce Delay
    document.getElementById('soundThreshold').addEventListener('input', function () {
        const soundThreshold = this.value;
        const xhr = new XMLHttpRequest();
        xhr.open('POST', '/setSoundThreshold', true);
        xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
        xhr.send('threshold=' + encodeURIComponent(soundThreshold));
    });

    document.getElementById('soundDebounce').addEventListener('input', function () {
        const soundDebounceDelay = this.value;
        const xhr = new XMLHttpRequest();
        xhr.open('POST', '/setSoundDebounceDelay', true);
        xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
        xhr.send('debounceDelay=' + encodeURIComponent(soundDebounceDelay));
    });

    document.getElementById('pirDebounce').addEventListener('input', function () {
        const pirDebounceDelay = this.value;
        const xhr = new XMLHttpRequest();
        xhr.open('POST', '/setPirDebounceDelay', true);
        xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
        xhr.send('pirDebounceDelay=' + encodeURIComponent(pirDebounceDelay));
    });

    // Function to toggle the device name to an input field when clicked
    function editDeviceName() {
        const deviceNameSpan = document.getElementById('deviceName');
        const deviceNameInput = document.getElementById('deviceNameInput');

        // Set the input value to the current device name
        deviceNameInput.value = deviceNameSpan.textContent;

        // Hide the span and show the input field
        deviceNameSpan.style.display = 'none';
        deviceNameInput.style.display = 'inline-block';
        deviceNameInput.focus();  // Automatically focus on the input field
    }

    // Function to save the device name when the input field loses focus or "Enter" is pressed
    function saveDeviceName() {
        const deviceNameSpan = document.getElementById('deviceName');
        const deviceNameInput = document.getElementById('deviceNameInput');
        const newDeviceName = deviceNameInput.value;

        // Update the span with the new device name
        deviceNameSpan.textContent = newDeviceName;

        // Update the other name field (chatNameField)
        const chatNameField = document.getElementById('chatNameField');
        if (chatNameField) {
            chatNameField.value = newDeviceName;  // Update the other field with the same device name
        }

        // Hide the input field and show the span again
        deviceNameInput.style.display = 'none';
        deviceNameSpan.style.display = 'inline-block';

        // Send the new device name to the server
        const xhr = new XMLHttpRequest();
        xhr.open('POST', '/setDeviceName', true);
        xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
        xhr.send('deviceName=' + encodeURIComponent(newDeviceName));
    }

    // Function to check if "Enter" was pressed
    function checkEnter(event) {
        if (event.key === "Enter") {
            saveDeviceName();
        }
    }

    // JavaScript to handle LED Enable/Disable
    document.getElementById('ledToggle').addEventListener('change', function () {
        const ledEnabled = this.checked ? '1' : '0';
        const xhr = new XMLHttpRequest();
        xhr.open('POST', '/setLedState', true);
        xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
        xhr.send('ledEnabled=' + ledEnabled);
    });

    // JavaScript for Mic On Color
    document.getElementById('micOnColor').addEventListener('change', function () {
        const micOnColor = this.value;
        const xhr = new XMLHttpRequest();
        xhr.open('POST', '/setMicOnColor', true);
        xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
        xhr.send('micOnColor=' + micOnColor);
    });

    // JavaScript for Mic Off Color
    document.getElementById('micOffColor').addEventListener('change', function () {
        const micOffColor = this.value;
        const xhr = new XMLHttpRequest();
        xhr.open('POST', '/setMicOffColor', true);
        xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
        xhr.send('micOffColor=' + micOffColor);
    });

    // JavaScript for Mic Ready Color
    document.getElementById('micReadyColor').addEventListener('change', function () {
        const micReadyColor = this.value;
        const xhr = new XMLHttpRequest();
        xhr.open('POST', '/setMicReadyColor', true);
        xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
        xhr.send('micReadyColor=' + micReadyColor);
    });



    // JavaScript to handle Theatre Chat On/Off
    document.getElementById('msgSendToggle').addEventListener('change', function () {
        const msgSend = this.checked ? '1' : '0';
        const xhr = new XMLHttpRequest();
        xhr.open('POST', '/setMessageSending', true);  // Ensure this route handles saving the state
        xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
        xhr.send('messageSending=' + msgSend);

        // Enable/disable the chat port, channel, and message fields based on toggle
        document.getElementById('chatPort').disabled = !this.checked;
        document.getElementById('chatChannel').disabled = !this.checked;
        document.getElementById('chatMessage').disabled = !this.checked;
    });

    // Handle Test OSC Message Button
    document.getElementById('sendTestOsc').addEventListener('click', function () {
        const chatMessage = document.getElementById('chatMessage').value;
        if (!chatMessage) {
            alert('Please enter a message to send.');
            return;
        }
        const xhr = new XMLHttpRequest();
        xhr.open('POST', '/sendTestOscMessage', true);
        xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
        xhr.send('message=' + encodeURIComponent(chatMessage));

        alert('Test OSC message sent: ' + chatMessage);
    });

    // OSC On/Off
    document.getElementById('oscToggle').addEventListener('change', function () {
        const oscReceive = this.checked ? '1' : '0';
        const xhr = new XMLHttpRequest();
        xhr.open('POST', '/setOscReceive', true);
        xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
        xhr.send('oscReceive=' + oscReceive);
    });

    document.getElementById('oscPort').addEventListener('change', function () {
        const oscPort = this.value;
        const xhr = new XMLHttpRequest();
        xhr.open('POST', '/setOscPort', true);
        xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
        xhr.send('port=' + oscPort);
    });

    document.querySelectorAll('input[name="orientation"]').forEach(radio => {
        radio.addEventListener('change', function () {
            const orientation = this.value;
            const xhr = new XMLHttpRequest();
            xhr.open('POST', '/setDeviceOrientation', true);
            xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
            xhr.send('orientation=' + orientation);
        });
    });

    // Updated Theatre Chat Configuration
    document.getElementById('chatPort').addEventListener('input', updateTheatreChatConfig);
    document.getElementById('chatChannel').addEventListener('input', updateTheatreChatConfig);
    document.getElementById('chatMessage').addEventListener('input', updateTheatreChatConfig);

    function updateTheatreChatConfig() {
        const chatPort = document.getElementById('chatPort').value;
        const chatChannel = document.getElementById('chatChannel').value;
        const chatMessage = document.getElementById('chatMessage').value;
        const chatName = document.getElementById('chatName').value;

        if (chatPort && chatChannel && chatMessage) {
            const xhr = new XMLHttpRequest();
            xhr.open('POST', '/setTheatreChatConfig', true);
            xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
            xhr.send(`port=${chatPort}&channel=${chatChannel}&name=${chatName}&message=${chatMessage}`);
        } else {
            console.error('All Theatre Chat fields must be filled before sending the request.');
        }
    }

    // JavaScript for LED Brightness Slider
    document.getElementById('ledBrightness').addEventListener('input', function () {
        const ledBrightness = this.value;
        document.getElementById('ledBrightnessValue').textContent = ledBrightness;

        const xhr = new XMLHttpRequest();
        xhr.open('POST', '/setLedBrightness', true);
        xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
        xhr.send('ledBrightness=' + ledBrightness);
    });

    // Function to update uptime
    function updateUptime() {
        fetch('/getUptime')
            .then(response => response.text())
            .then(data => {
                document.getElementById('uptime').innerText = data;
            })
            .catch(err => {
                console.error('Error fetching uptime:', err);
                document.getElementById('uptime').innerText = 'Error';
            });
    }

    // Update uptime every 5 seconds
    setInterval(updateUptime, 5000);
    updateUptime();

    // Function to update memory and CPU usage
    function updateUsage() {
        fetch('/getUsage')
        .then(response => response.json())
        .then(data => {
            document.getElementById('memoryUsage').innerText = data.memoryUsage + '%';
            document.getElementById('cpuUsage').innerText = data.cpuUsage + '%';
        })
        .catch(err => {
            console.error('Error fetching usage:', err);
            document.getElementById('memoryUsage').innerText = 'Error';
            document.getElementById('cpuUsage').innerText = 'Error';
        });
    }

    // Update memory and CPU usage every 5 seconds
    setInterval(updateUsage, 5000);
    updateUsage();

    // Function to load the settings from ESP32 when the page loads
    window.onload = function () {
        const xhr = new XMLHttpRequest();
        xhr.open('GET', '/getSettings', true);
        xhr.onload = function () {
            if (xhr.status === 200) {
                const settings = JSON.parse(xhr.responseText);
                console.log("Settings loaded:", settings);

                // Set the device name to theatreChatName
                const deviceName = document.getElementById('deviceName');
                if (deviceName) {
                    deviceName.textContent = settings.theatreChatName;
                }

                // Set the IP Address and make it clickable
                const deviceIP = document.getElementById('deviceIP');
                if (deviceIP) {
                    const ipAddress = location.hostname;
                    deviceIP.textContent = ipAddress;
                    deviceIP.href = `http://${ipAddress}:81/`;
                }

                // Set the values for each control based on the JSON response
                document.getElementById('debugModeToggle').checked = settings.debugMode;
                document.getElementById('flip').checked = settings.cameraFlip;
                document.getElementById('mirror').checked = settings.cameraMirror;

                // Update slider values and display them in the span elements
                const brightnessSlider = document.getElementById('brightness');
                const brightnessValue = document.getElementById('brightnessValue');
                brightnessSlider.value = settings.brightness;
                brightnessValue.textContent = settings.brightness;

                const contrastSlider = document.getElementById('contrast');
                const contrastValue = document.getElementById('contrastValue');
                contrastSlider.value = settings.contrast;
                contrastValue.textContent = settings.contrast;

                const frameRate = document.getElementById('frameRate');
                const frameRateValue = document.getElementById('frameRateValue');

                const qualitySlider = document.getElementById('quality');
                const qualityValue = document.getElementById('qualityValue');
                qualitySlider.value = settings.quality;
                qualityValue.textContent = settings.quality;

                // If you have more sliders like whiteBalance, repeat the same logic for them
                // const whiteBalanceSlider = document.getElementById('whiteBalance');
                // const whiteBalanceValue = document.getElementById('whiteBalanceValue');
                // whiteBalanceSlider.value = settings.whiteBalance;
                // whiteBalanceValue.textContent = settings.whiteBalance;

                document.getElementById('irOn').checked = settings.irEnabled;
                document.getElementById('msgSendToggle').checked = settings.theatreChatEnabled;
                document.getElementById('chatPort').value = settings.theatreChatPort;
                document.getElementById('chatChannel').value = settings.theatreChatChannel;
                document.getElementById('chatMessage').value = settings.theatreChatMessage;
                document.getElementById('oscPort').value = settings.oscPort;

                document.getElementById('soundThreshold').value = settings.SOUND_THRESHOLD;
                document.getElementById('soundDebounce').value = settings.SOUND_DEBOUNCE_DELAY;
                document.getElementById('pirDebounce').value = settings.PIR_DEBOUNCE_DELAY;
                document.getElementById('soundDetectionToggle').checked = settings.soundDetectionToggle;
                document.getElementById('PIRDetectionToggle').checked = settings.PIRDetectionToggle;

                // Grey out the theatre chat name field
                const chatNameField = document.getElementById('chatName');
                if (chatNameField) {
                    chatNameField.value = settings.theatreChatName;
                    chatNameField.readOnly = true;
                    chatNameField.style.backgroundColor = '#e0e0e0';
                    chatNameField.style.color = '#333';
                }

                // Set saturation slider
                const saturationSlider = document.getElementById('saturation');
                const saturationValue = document.getElementById('saturationValue');
                saturationSlider.value = settings.saturation;
                saturationValue.textContent = settings.saturation;

                // Set special effect dropdown
                const specialEffectSelect = document.getElementById('specialEffect');
                specialEffectSelect.value = settings.specialEffect;

                // Set white balance checkbox
                document.getElementById('whitebal').checked = settings.whitebal;

                // Set AWB gain checkbox
                document.getElementById('awbGain').checked = settings.awbGain;

                // Set white balance mode dropdown
                const wbModeSelect = document.getElementById('wbMode');
                wbModeSelect.value = settings.wbMode;

                // Update LED Brightness slider and value
                const ledBrightnessSlider = document.getElementById('ledBrightness');
                const ledBrightnessValue = document.getElementById('ledBrightnessValue');
                ledBrightnessSlider.value = settings.ledBrightness;
                ledBrightnessValue.textContent = settings.ledBrightness;

                const micOnColorInput = document.getElementById('micOnColor');
                const micOffColorInput = document.getElementById('micOffColor');
                const micReadyColorInput = document.getElementById('micReadyColor');
                const ledToggle = document.getElementById('ledToggle');

                document.getElementById('oscToggle').checked = settings.oscReceive;

                // Set the device orientation radio button based on the settings
                const orientationValue = settings.deviceOrientation;
                const orientationRadioButtons = document.getElementsByName('orientation');
                orientationRadioButtons.forEach(button => {
                    if (parseInt(button.value) === orientationValue) {
                        button.checked = true;  // Select the radio button that matches the orientation
                    }
                });

            } else {
                console.error("Failed to load settings, status:", xhr.status);
            }
        };

        xhr.onerror = function () {
            console.error("Error fetching settings.");
        };

        xhr.send();
    };
</script>





</body>
</html>

)rawliteral";
