# Backstage Cameras

Create an internal-facing website (hosted onsite, not web-facing) with the following features:

## 1. Video Feed Display
- Displays multiple video feeds simultaneously.
- The video feeds are displayed on a **flexible grid** that resizes based on the number of feeds.
- Users can **move video feeds** around to different grid tiles.

## 2. Toolbar
- An **auto-hiding tool bar** on the left side (with a hamburger button for mobile devices).
- The tool bar allows users to **add video feeds by IP address**.
- The tool bar will contain a **list of video feeds** with the option to easily **remove feeds**.
- The toolbar will also include an option to change the **admin password** for website login.

## 3. Video Feed Handling
- Video feeds will be located at: `http://[ip address]:81/`.
- Connect to the HTTP endpoint, read the multipart MJPEG stream, and handle each JPEG frame separated by `--frame` boundaries. Keep the connection open to receive continuous frames.
- If a feed fails to load or loses connection, the system should **continuously retry** to reconnect.

## 4. Device Information
- **Above each video feed**, display the **device name** (retrieved from `/getSettings`) and **battery level** (from `/getBatteryPercentage`).
- Clicking the device name should take the user to the settings page of the device (at `http://[ip address]/`).

## 5. Login and Security
- The website will require a **general admin login** (password changeable via the toolbar).

## 6. Performance
- Optimize for **low latency**, prioritizing fast response times over smooth playback.

## 7. Visual Design
- A **simple, functional** layout with **dark colors** is preferred.
