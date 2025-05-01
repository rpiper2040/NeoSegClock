#ifndef INDEX_HTML_H
#define INDEX_HTML_H

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>NeoSegClock Control Panel</title>
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
            font-family: Arial, sans-serif;
        }
        body {
            background: #1a1a1a;
            color: #e0e0e0;
            padding: 20px;
            line-height: 1.6;
        }
        .container {
            max-width: 800px;
            margin: 0 auto;
        }
        header {
            text-align: center;
            margin-bottom: 30px;
        }
        h1 {
            font-size: 2rem;
            color: #00b7eb;
        }
        h2 {
            font-size: 1.5rem;
            margin-bottom: 15px;
            color: #00b7eb;
            text-align: center;
        }
        section {
            background: #2a2a2a;
            padding: 20px;
            margin-bottom: 20px;
            border-radius: 8px;
            box-shadow: 0 2px 5px rgba(0, 0, 0, 0.5);
        }
        .general-section {
            display: grid;
            gap: 15px;
        }
        .controls {
            display: flex;
            flex-wrap: wrap;
            gap: 20px;
            align-items: center;
        }
        .time-display, .rgb-display {
            font-size: 1.2rem;
            margin-right: 20px;
        }
        .time-display span, .rgb-display span {
            font-weight: bold;
            color: #00b7eb;
        }
        form {
            display: flex;
            flex-wrap: wrap;
            gap: 10px;
            align-items: center;
        }
        input[type="time"], input[type="text"], input[type="password"], select, input[type="color"] {
            padding: 8px;
            border: none;
            border-radius: 4px;
            background: #3a3a3a;
            color: #e0e0e0;
            font-size: 1rem;
        }
        input[type="color"] {
            padding: 2px;
            width: 50px;
            height: 30px;
        }
        button {
            padding: 8px 16px;
            background: #00b7eb;
            border: none;
            border-radius: 4px;
            color: #fff;
            cursor: pointer;
            font-size: 1rem;
        }
        button:hover {
            background: #0097c7;
        }
        .alarm {
            display: grid;
            grid-template-columns: auto 1fr 1fr 1fr auto auto;
            gap: 10px;
            align-items: center;
            padding: 10px;
            border-bottom: 1px solid #3a3a3a;
        }
        .alarm:last-child {
            border-bottom: none;
        }
        .add-alarm-btn {
            margin-top: 15px;
            width: 100%;
            max-width: 200px;
            display: block;
            margin-left: auto;
            margin-right: auto;
        }
        .toggle {
            position: relative;
            display: inline-block;
            width: 40px;
            height: 20px;
        }
        .toggle input {
            opacity: 0;
            width: 0;
            height: 0;
        }
        .slider {
            position: absolute;
            cursor: pointer;
            top: 0;
            left: 0;
            right: 0;
            bottom: 0;
            background: #3a3a3a;
            transition: 0.4s;
            border-radius: 20px;
        }
        .slider:before {
            position: absolute;
            content: "";
            height: 16px;
            width: 16px;
            left: 2px;
            bottom: 2px;
            background: #e0e0e0;
            transition: 0.4s;
            border-radius: 50%;
        }
        input:checked + .slider {
            background: #00b7eb;
        }
        input:checked + .slider:before {
            transform: translateX(20px);
        }
        .delete-btn {
            background: #e63946;
        }
        .delete-btn:hover {
            background: #b32d38;
        }
        /* Processing Popup Styles */
        .processing-popup {
            display: none;
            position: fixed;
            top: 0;
            left: 0;
            width: 100%;
            height: 100%;
            background: rgba(0, 0, 0, 0.5);
            z-index: 1000;
            justify-content: center;
            align-items: center;
        }
        .processing-content {
            background: #2a2a2a;
            padding: 20px;
            border-radius: 8px;
            display: flex;
            flex-direction: column;
            align-items: center;
            gap: 10px;
        }
        .spinner {
            border: 4px solid #e0e0e0;
            border-top: 4px solid #00b7eb;
            border-radius: 50%;
            width: 30px;
            height: 30px;
            animation: spin 1s linear infinite;
        }
        @keyframes spin {
            0% { transform: rotate(0deg); }
            100% { transform: rotate(360deg); }
        }
        .processing-text {
            font-size: 1rem;
            color: #e0e0e0;
        }
        /* Message Popup Styles */
        .message-popup {
            display: none;
            position: fixed;
            top: 0;
            left: 0;
            width: 100%;
            height: 100%;
            background: rgba(0, 0, 0, 0.5);
            z-index: 1000;
            justify-content: center;
            align-items: center;
        }
        .message-content {
            background: #2a2a2a;
            padding: 20px;
            border-radius: 8px;
            max-width: 400px;
            text-align: center;
            display: flex;
            flex-direction: column;
            gap: 15px;
        }
        .message-text {
            font-size: 1rem;
            color: #e0e0e0;
            white-space: pre-wrap; /* Preserve formatting for JSON */
        }
        .message-button {
            padding: 8px 16px;
            background: #00b7eb;
            border: none;
            border-radius: 4px;
            color: #fff;
            cursor: pointer;
            font-size: 1rem;
        }
        .message-button:hover {
            background: #0097c7;
        }
        @media (max-width: 600px) {
            .alarm {
                grid-template-columns: 1fr;
                gap: 5px;
            }
            form, .controls {
                flex-direction: column;
            }
            button {
                width: 100%;
            }
            .time-display, .rgb-display {
                margin-right: 0;
            }
            .message-content {
                max-width: 90%;
            }
        }
    </style>
</head>
<body>
    <div class="container">
        <header>
            <h1>NeoSegClock Control Panel</h1>
        </header>

        <section class="general-section">
            <h2>General</h2>
            <div class="controls">
                <div class="time-display">Current Time: <span id="current-time">12:34:56</span></div>
                <form id="time-form" onsubmit="setTime(event)">
                    <input type="time" id="new-time" step="1" required>
                    <button type="submit">Set Time</button>
                </form>
            </div>
            <div class="controls">
                <div class="rgb-display">Current RGB: <span id="current-rgb" style="color: #ff0000;">#FF0000</span></div>
                <form id="rgb-form" onsubmit="setRGB(event)">
                    <input type="color" id="new-rgb" value="#ff0000">
                    <button type="submit">Set RGB</button>
                </form>
            </div>
        </section>

        <section class="alarms-section">
            <h2>Alarms</h2>
            <div class="alarm">
                <label class="toggle">
                    <input type="checkbox" checked>
                    <span class="slider"></span>
                </label>
                <input type="time" value="07:00" disabled>
                <input type="color" value="#00ff00" disabled>
                <select disabled>
                    <option value="0" selected>Tone 0</option>
                    <option value="1">Tone 1</option>
                    <option value="2">Tone 2</option>
                    <option value="3">Tone 3</option>
                    <option value="4">Tone 4</option>
                </select>
                <button class="delete-btn" onclick="deleteAlarm(this)">Delete</button>
                <button class="edit-btn" onclick="editAlarm(this)">Edit</button>
            </div>
            <div class="alarm">
                <label class="toggle">
                    <input type="checkbox">
                    <span class="slider"></span>
                </label>
                <input type="time" value="08:30" disabled>
                <input type="color" value="#0000ff" disabled>
                <select disabled>
                    <option value="0">Tone 0</option>
                    <option value="1" selected>Tone 1</option>
                    <option value="2">Tone 2</option>
                    <option value="3">Tone 3</option>
                    <option value="4">Tone 4</option>
                </select>
                <button class="delete-btn" onclick="deleteAlarm(this)">Delete</button>
                <button class="edit-btn" onclick="editAlarm(this)">Edit</button>
            </div>
            <div class="alarm">
                <label class="toggle">
                    <input type="checkbox" checked>
                    <span class="slider"></span>
                </label>
                <input type="time" value="18:45" disabled>
                <input type="color" value="#ffff00" disabled>
                <select disabled>
                    <option value="0">Tone 0</option>
                    <option value="1">Tone 1</option>
                    <option value="2" selected>Tone 2</option>
                    <option value="3">Tone 3</option>
                    <option value="4">Tone 4</option>
                </select>
                <button class="delete-btn" onclick="deleteAlarm(this)">Delete</button>
                <button class="edit-btn" onclick="editAlarm(this)">Edit</button>
            </div>
            <button class="add-alarm-btn" onclick="addAlarm()">Add Alarm</button>
        </section>

        <section class="settings-section">
            <h2>Settings</h2>
            <form id="settings-form" onsubmit="saveSettings(event)">
                <label>WiFi SSID:</label>
                <input type="text" id="ssid" placeholder="Enter SSID" required>
                <label>WiFi Password:</label>
                <input type="password" id="wifi-password" placeholder="Enter Password">
                <label>Time Format:</label>
                <select id="time-format">
                    <option value="24">24 Hour</option>
                    <option value="12">12 Hour</option>
                </select>
                <label>NTP Server:</label>
                <input type="text" id="ntp-server" placeholder="pool.ntp.org" value="pool.ntp.org">
                <label>Default RGB:</label>
                <input type="color" id="default-rgb" value="#ffffff">
                <label>mDNS Name:</label>
                <input type="text" id="mdns-name" placeholder="esp32.local" value="esp32.local">
                <button type="submit">Save Settings</button>
            </form>
        </section>
    </div>

    <!-- Processing Popup -->
    <div class="processing-popup" id="processing-popup">
        <div class="processing-content">
            <div class="spinner"></div>
            <div class="processing-text">Processing...</div>
        </div>
    </div>

    <!-- Message Popup -->
    <div class="message-popup" id="message-popup">
        <div class="message-content">
            <div class="message-text" id="message-text"></div>
            <button class="message-button" onclick="hideMessagePopup()">OK</button>
        </div>
    </div>

    <script>
        // Helper function to convert hex color to RGB
        function hexToRGB(hex) {
            const r = parseInt(hex.slice(1, 3), 16);
            const g = parseInt(hex.slice(3, 5), 16);
            const b = parseInt(hex.slice(5, 7), 16);
            return `${r},${g},${b}`;
        }

        // Show processing popup
        function showProcessingPopup() {
            document.getElementById('processing-popup').style.display = 'flex';
        }

        // Hide processing popup
        function hideProcessingPopup() {
            document.getElementById('processing-popup').style.display = 'none';
        }

        // Show message popup
        function showMessagePopup(message) {
            document.getElementById('message-text').textContent = message;
            document.getElementById('message-popup').style.display = 'flex';
        }

        // Hide message popup
        function hideMessagePopup() {
            document.getElementById('message-popup').style.display = 'none';
        }

        // Centralized API call function
        async function doApiCall(url) {
            try {
                showProcessingPopup();
                const response = await fetch(url);
                const data = await response.json();
                return data;
            } catch (error) {
                console.error('API call failed:', error);
                throw error;
            } finally {
                hideProcessingPopup();
            }
        }

        async function setTime(event) {
            event.preventDefault();
            const time = document.getElementById('new-time').value;
            const [hours, minutes, seconds] = time.split(':').map(Number);
            const url = `/api?cmd=setTime&hh=${hours}&mm=${minutes}&ss=${seconds}`;
            
            try {
                const data = await doApiCall(url);
                if (data.status === 'ok') {
                    document.getElementById('current-time').textContent = time;
                    showMessagePopup('Time set to ' + time);
                } else {
                    showMessagePopup('Failed to set time: ' + JSON.stringify(data));
                }
            } catch (error) {
                showMessagePopup('Error setting time');
            }
        }

        async function setRGB(event) {
            event.preventDefault();
            const rgb = document.getElementById('new-rgb').value;
            const rgbNoHash = rgb.replace('#', '');
            const url = `/api?cmd=setColor&color=${rgbNoHash}`;
            
            try {
                const data = await doApiCall(url);
                if (data.status === 'ok') {
                    document.getElementById('current-rgb').textContent = rgb;
                    document.getElementById('current-rgb').style.color = rgb;
                    document.getElementById('new-rgb').value = rgb;
                    showMessagePopup('RGB set to ' + rgb);
                } else {
                    showMessagePopup('Failed to set color: ' + JSON.stringify(data));
                }
            } catch (error) {
                showMessagePopup('Error setting color');
            }
        }

        async function updateAlarm(alarmElement, isToggle = false) {
            const index = Array.from(document.querySelectorAll('.alarm')).indexOf(alarmElement);
            const toggleInput = alarmElement.querySelector('input[type="checkbox"]');
            let url = `/api?cmd=edit&index=${index}`;
            
            if (isToggle) {
                url += `&enable=${toggleInput.checked}`;
            } else {
                const timeInput = alarmElement.querySelector('input[type="time"]');
                const colorInput = alarmElement.querySelector('input[type="color"]');
                const toneSelect = alarmElement.querySelector('select');
                const time = timeInput.value;
                const colorRGB = hexToRGB(colorInput.value);
                const tone = toneSelect.value;
                url += `&time=${time}&color=${colorRGB}&tone=${tone}&enable=${toggleInput.checked}`;
            }

            try {
                const data = await doApiCall(url);
                if (data.status === 'ok') {
                    const action = isToggle ? 'toggled' : 'updated';
                    showMessagePopup(`Alarm ${index} ${action}: ${url}`);
                } else {
                    showMessagePopup(`Failed to update alarm: ${JSON.stringify(data)}`);
                }
            } catch (error) {
                showMessagePopup('Error updating alarm');
            }
        }

        async function removeAlarm(button) {
            const alarm = button.parentElement;
            const index = Array.from(document.querySelectorAll('.alarm')).indexOf(alarm);
            const url = `/api?cmd=remove&index=${index}`;
            
            try {
                const data = await doApiCall(url);
                if (data.status === 'ok') {
                    alarm.remove();
                    showMessagePopup(`Alarm ${index} deleted`);
                } else {
                    showMessagePopup(`Failed to delete alarm: ${JSON.stringify(data)}`);
                }
            } catch (error) {
                showMessagePopup('Error deleting alarm');
            }
        }

        async function addAlarmServer(button) {
            const alarm = button.parentElement;
            const timeInput = alarm.querySelector('input[type="time"]');
            const colorInput = alarm.querySelector('input[type="color"]');
            const toneSelect = alarm.querySelector('select');
            const [hour, minute] = timeInput.value.split(':').map(Number);
            const [r, g, b] = hexToRGB(colorInput.value).split(',').map(Number);
            const tone = parseInt(toneSelect.value);
            const args = `${hour}+${minute}+${r}+${g}+${b}+${tone}`;
            const url = `/api?cmd=add&args=${args}`;

            try {
                const data = await doApiCall(url);
                if (data.status === 'ok') {
                    await fetchAlarms();
                    showMessagePopup(`Alarm added: ${args}`);
                } else {
                    showMessagePopup(`Failed to add alarm: ${JSON.stringify(data)}`);
                }
            } catch (error) {
                showMessagePopup('Error adding alarm');
            }
        }

        function deleteAlarm(button) {
            const alarm = button.parentElement;
            if (alarm.hasAttribute('data-new')) {
                if (confirm('Delete this unsaved alarm?')) {
                    alarm.remove();
                }
            } else {
                if (confirm('Delete this alarm?')) {
                    removeAlarm(button);
                }
            }
        }

        function editAlarm(button) {
            const alarm = button.parentElement;
            const timeInput = alarm.querySelector('input[type="time"]');
            const colorInput = alarm.querySelector('input[type="color"]');
            const toneSelect = alarm.querySelector('select');
            const isEditing = !timeInput.disabled;

            if (isEditing) {
                if (alarm.hasAttribute('data-new')) {
                    addAlarmServer(button);
                } else {
                    timeInput.disabled = true;
                    colorInput.disabled = true;
                    toneSelect.disabled = true;
                    button.textContent = 'Edit';
                    updateAlarm(alarm);
                }
            } else {
                timeInput.disabled = false;
                colorInput.disabled = false;
                toneSelect.disabled = false;
                button.textContent = 'Save';
            }
        }

        function addAlarm() {
            const alarmsSection = document.querySelector('.alarms-section');
            const newAlarm = document.createElement('div');
            newAlarm.className = 'alarm';
            newAlarm.setAttribute('data-new', 'true');
            
            const now = new Date();
            const hours = now.getHours().toString().padStart(2, '0');
            const minutes = now.getMinutes().toString().padStart(2, '0');
            const defaultTime = `${hours}:${minutes}`;

            newAlarm.innerHTML = `
                <label class="toggle">
                    <input type="checkbox">
                    <span class="slider"></span>
                </label>
                <input type="time" value="${defaultTime}">
                <input type="color" value="#ffffff">
                <select>
                    <option value="0" selected>Tone 0</option>
                    <option value="1">Tone 1</option>
                    <option value="2">Tone 2</option>
                    <option value="3">Tone 3</option>
                    <option value="4">Tone 4</option>
                </select>
                <button class="delete-btn" onclick="deleteAlarm(this)">Delete</button>
                <button class="edit-btn" onclick="editAlarm(this)">Save</button>
            `;

            alarmsSection.insertBefore(newAlarm, document.querySelector('.add-alarm-btn'));
        }

        function saveSettings(event) {
            event.preventDefault();
            const settings = {
                ssid: document.getElementById('ssid').value,
                password: document.getElementById('wifi-password').value,
                timeFormat: document.getElementById('time-format').value,
                ntpServer: document.getElementById('ntp-server').value,
                defaultRGB: document.getElementById('default-rgb').value,
                mdnsName: document.getElementById('mdns-name').value
            };
            showMessagePopup('Settings saved:\n' + JSON.stringify(settings, null, 2));
        }

        async function fetchTime() {
            try {
                const data = await doApiCall('/api?cmd=getTime');
                if (data.status === 'ok' && data.time) {
                    document.getElementById('current-time').textContent = data.time;
                }
            } catch (error) {
                console.error('Failed to fetch time:', error);
            }
        }

        async function fetchColor() {
            try {
                const data = await doApiCall('/api?cmd=getColor');
                if (data.status === 'ok' && data.color) {
                    document.getElementById('current-rgb').textContent = data.color;
                    document.getElementById('current-rgb').style.color = data.color;
                    document.getElementById('new-rgb').value = data.color;
                }
            } catch (error) {
                console.error('Failed to fetch color:', error);
            }
        }

        async function fetchAlarms() {
            try {
                const data = await doApiCall('/api?cmd=list');
                if (data.status === 'ok' && data.response) {
                    const alarmsSection = document.querySelector('.alarms-section');
                    const existingAlarms = alarmsSection.querySelectorAll('.alarm');
                    existingAlarms.forEach(alarm => alarm.remove());

                    data.response.forEach(alarmData => {
                        const newAlarm = document.createElement('div');
                        newAlarm.className = 'alarm';
                        const time = `${alarmData.hour.toString().padStart(2, '0')}:${alarmData.minute.toString().padStart(2, '0')}`;
                        const checked = alarmData.enabled ? 'checked' : '';
                        newAlarm.innerHTML = `
                            <label class="toggle">
                                <input type="checkbox" ${checked} onchange="if (!this.parentElement.parentElement.hasAttribute('data-new')) updateAlarm(this.parentElement.parentElement, true)">
                                <span class="slider"></span>
                            </label>
                            <input type="time" value="${time}" disabled>
                            <input type="color" value="${alarmData.color}" disabled>
                            <select disabled>
                                <option value="0" ${alarmData.tone === 0 ? 'selected' : ''}>Tone 0</option>
                                <option value="1" ${alarmData.tone === 1 ? 'selected' : ''}>Tone 1</option>
                                <option value="2" ${alarmData.tone === 2 ? 'selected' : ''}>Tone 2</option>
                                <option value="3" ${alarmData.tone === 3 ? 'selected' : ''}>Tone 3</option>
                                <option value="4" ${alarmData.tone === 4 ? 'selected' : ''}>Tone 4</option>
                            </select>
                            <button class="delete-btn" onclick="deleteAlarm(this)">Delete</button>
                            <button class="edit-btn" onclick="editAlarm(this)">Edit</button>
                        `;
                        alarmsSection.insertBefore(newAlarm, document.querySelector('.add-alarm-btn'));
                    });
                }
            } catch (error) {
                console.error('Failed to fetch alarms:', error);
            }
        }

        function updateTime() {
            const timeSpan = document.getElementById('current-time');
            let [hours, minutes, seconds] = timeSpan.textContent.split(':').map(Number);
            seconds++;
            if (seconds >= 60) {
                seconds = 0;
                minutes++;
            }
            if (minutes >= 60) {
                minutes = 0;
                hours++;
            }
            if (hours >= 24) hours = 0;
            timeSpan.textContent = `${hours.toString().padStart(2, '0')}:${minutes.toString().padStart(2, '0')}:${seconds.toString().padStart(2, '0')}`;
        }

        window.onload = async function() {
            await Promise.all([fetchTime(), fetchColor(), fetchAlarms()]);
            setInterval(updateTime, 1000);
        };
    </script>
</body>
</html>
)rawliteral";

#endif // INDEX_HTML_H
