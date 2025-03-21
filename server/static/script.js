// script.js

let activeDraggable = null;
let offsetX, offsetY;
let isResizing = false;
let initialWidth, initialHeight;
let initialMouseX, initialMouseY;
let cameraRecordingStates = {};
let cameraSettingsCache = {};
let batteryCache = {};
let toolbarLocked = false;

// Adjust initial positions as needed
//const left = (index % 3) * 200; // Example value
//const top = Math.floor(index / 3) * 150; // Example value

const socket = io();


// Function to fetch cameras and render them
function fetchAndRenderCameras() {
    return fetch('/get_last_scene')
        .then(response => response.json())
        .then(scene => {
            console.log("Scene data received:", scene);
            if (scene.error) {
                return fetch('/get_cameras')
                    .then(response => response.json())
                    .then(cameras => {
                        renderCameras(cameras);
                        return cameras; // Return cameras data
                    });
            } else {
                // Update current scene label
                updateCurrentSceneLabel(scene.sceneNumber, scene.sceneName);

                renderCameras(scene.cameras);
                return scene.cameras; // Return cameras data
            }
        })
        .catch(error => {
            console.error('Error fetching last scene:', error);
        });
}
/*
// Function to fetch cameras and render them
function fetchAndRenderCameras() {
    fetch('/get_last_scene')
        .then(response => response.json())
        .then(scene => {
            console.log("Scene data received:", scene);
            if (scene.error) {
                fetchCameras();  // If no last scene, fetch default cameras
            } else {
                // Update current scene label
                updateCurrentSceneLabel(scene.sceneNumber, scene.sceneName);

                const cameraContainer = document.getElementById('cameraContainer');
                cameraContainer.innerHTML = '';  // Clear existing content

                scene.cameras.forEach((camera, index) => {
                    // Make sure the camera name is rendered
                    renderCamera(camera, index);
                });

                setupEventDelegation();  // Set up drag/resize events
            }
        })
        .catch(error => {
            console.error('Error fetching last scene:', error);
        });
}
*/


//function fetchAndRenderCameras(scene) {
//    const cameraContainer = document.getElementById('cameraContainer');
//    cameraContainer.innerHTML = ''; // Clear existing content
//
//    scene.cameras.forEach((camera, index) => {
//        renderCamera(camera, index);
//    });
//
//    // Update the camera list
//    updateCameraList(scene.cameras);
//}
function renderCameras(cameras) {
    const cameraContainer = document.getElementById('cameraContainer');
    cameraContainer.innerHTML = '';  // Clear existing content

    cameras.forEach((camera, index) => {
        // Make sure the camera name is rendered
        renderCamera(camera, index);
    });

    setupEventDelegation();  // Set up drag/resize events
}

function fetchCameras() {
    fetch('/get_cameras')
        .then(response => response.json())
        .then(cameras => {
            const cameraContainer = document.getElementById('cameraContainer');
            cameraContainer.innerHTML = ''; // Clear existing content

            cameras.forEach((camera, index) => {
                renderCamera(camera, index);
            });

            // Set up event listeners for dragging and resizing
            setupEventDelegation();
        })
        .catch(error => {
            console.error('Error fetching cameras:', error);
        });
}

/*
function fetchCameras() {
    fetch('/get_cameras')
        .then(response => response.json())
        .then(cameras => {
            const cameraContainer = document.getElementById('cameraContainer');
            cameraContainer.innerHTML = ''; // Clear existing content

            cameras.forEach((camera, index) => {
                renderCamera(camera, index);
            });

            // Set up event listeners for dragging and resizing
            setupEventDelegation();
        })
        .catch(error => {
            console.error('Error fetching cameras:', error);
        });
}
*/

function renderCamera(camera, index) {
    // Add the following IP validation check at the beginning
    if (!camera.ip || typeof camera.ip !== 'string' || camera.ip.trim() === '') {
        console.error("Camera at index " + index + " has no valid IP. Skipping render.");
        return; // Do not render this camera if IP is invalid
    }
    const cameraContainer = document.getElementById('cameraContainer');

    const draggable = document.createElement('div');
    draggable.classList.add('draggable');
    draggable.id = `draggable_${index}`;

    draggable.style.zIndex = camera.zIndex ?? 0;

    if (!camera.visible) draggable.style.display = 'none';

    // Set position and size
    //draggable.style.left = `${camera.position.left || (index % 3) * 350}px`;
    draggable.style.left = `${camera.position.left ?? 0}px`;
    //draggable.style.top = `${camera.position.top || Math.floor(index / 3) * 300}px`;
    draggable.style.top = `${camera.position.top ?? 0}px`;
    draggable.style.width = `${camera.size.width || 320}px`;
    draggable.style.height = `${camera.size.height || 240}px`;

    // Create the image element for MJPEG stream
    const img = document.createElement('img');
    img.id = `camera_${index}`;
    // img.src = '/static/images/placeholder.jpg';  // Or leave it empty
    //img.src = '/static/images/camera_unavailable.jpg';
    img.src = `/camera_stream/${camera.ip}`;
    img.classList.add('camera-stream');
    img.setAttribute('data-ip', camera.ip);  // Set the IP address
    //console.log("Rendering camera with IP:", camera.ip);
    img.onerror = function() { // &&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&ADDED&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&
        img.src = '/static/images/camera_unavailable.jpg';
    };
    //img.setAttribute('data-index', index); //Added to help refresh button in fetchCameraSettings



    // Create overlay for camera label
    const overlay = document.createElement('div');
    overlay.classList.add('overlay');

    // Ensure the name is maintained from the camera object if already set
    const cameraName = camera.name || 'Unnamed Camera';  // Use camera name if it exists

    // Create label element for the camera name
    const label = document.createElement('a');
    label.classList.add('camera-label');
    label.textContent = cameraName;  // Set the name from the camera object
    label.href = `http://${camera.ip}/`;  // Set the link to the camera's IP address
    label.target = '_blank';  // Open in a new tab
    label.style.textDecoration = 'none';  // Remove underline for link
    label.style.color = '#fff';  // Ensure text is white

    // Append label to the overlay
    //overlay.appendChild(label);

    // Create a container just for the two layer buttons
    const layerBtnContainer = document.createElement('div');
    layerBtnContainer.classList.add('layer-button-container'); 
    // This container will hold the ▲ and ▼ buttons side by side

    // Create the bring-to-front and send-to-back buttons
    const bringFrontBtn = document.createElement('button');
    bringFrontBtn.textContent = '▲';
    bringFrontBtn.title = 'Bring to front';
    bringFrontBtn.classList.add('layer-button');

    // ➜ ADD AN EVENT LISTENER to actually bring to front
    bringFrontBtn.addEventListener('click', (event) => {
        event.stopPropagation(); // Prevents triggering drag start
        bringToFront(draggable);
    });

    const sendBackBtn = document.createElement('button');
    sendBackBtn.textContent = '▼';
    sendBackBtn.title = 'Send to back';
    sendBackBtn.classList.add('layer-button');

    // ➜ ADD AN EVENT LISTENER to actually send to back
    sendBackBtn.addEventListener('click', (event) => {
        event.stopPropagation();
        sendToBack(draggable);
    });

    // Append the buttons to the container
    layerBtnContainer.appendChild(bringFrontBtn);
    layerBtnContainer.appendChild(sendBackBtn);

    // Append the container to the overlay (so both buttons appear in the overlay)
    overlay.appendChild(layerBtnContainer);

    // Append the image, overlay to the draggable div
    draggable.appendChild(img);
    draggable.appendChild(overlay);

    // Append the draggable element to the camera container
    cameraContainer.appendChild(draggable);



    // Add a resize handle for resizing functionality
    const resizeHandle = document.createElement('div');
    resizeHandle.classList.add('resize-handle');
    draggable.appendChild(resizeHandle);

    fetchCameraSettings(camera.ip, overlay, img);
}


// Helper to find the highest zIndex among .draggable elements
function getMaxZIndex() {
    let maxZ = 0;
    document.querySelectorAll('.draggable').forEach(elem => {
        const z = parseInt(elem.style.zIndex) || 0;
        if (z > maxZ) {
            maxZ = z;
        }
    });
    return maxZ;
}

// Bring a camera to the front
function bringToFront(draggable) {
    const maxZ = getMaxZIndex();
    const newZ = maxZ + 1;
    draggable.style.zIndex = newZ;

    // We can also read the IP, position, size, then do updateCamera
    const img = draggable.querySelector('img.camera-stream');
    const ip = img.getAttribute('data-ip');

    // Gather position, size
    const position = {
        left: parseInt(draggable.style.left, 10) || 0,
        top: parseInt(draggable.style.top, 10) || 0
    };
    const size = {
        width: draggable.offsetWidth,
        height: draggable.offsetHeight
    };

    // Call the server
    updateCameraWithZIndex(ip, position, size, newZ);
}

// Send a camera to the back
function sendToBack(draggable) {
    const newZ = 0;
    draggable.style.zIndex = newZ;

    // Similarly, read the IP, position, size
    const img = draggable.querySelector('img.camera-stream');
    const ip = img.getAttribute('data-ip');

    const position = {
        left: parseInt(draggable.style.left, 10) || 0,
        top: parseInt(draggable.style.top, 10) || 0
    };
    const size = {
        width: draggable.offsetWidth,
        height: draggable.offsetHeight
    };

    updateCameraWithZIndex(ip, position, size, newZ);
}

function updateCameraWithZIndex(ip, position, size, zIndex) {
    fetch('/update_camera', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
            ip: ip,
            position: position,
            size: size,
            zIndex: zIndex   // <--- include in the body
        })
    })
    .then(response => {
        if (!response.ok) {
            console.error('Failed to update camera with zIndex');
        }
    })
    .catch(error => {
        console.error('Error updating camera with zIndex:', error);
    });
}


function startImageRefresh() {
    setInterval(() => {
        const images = document.querySelectorAll('img.camera-stream');
        images.forEach(img => {
            const currentSrc = img.src.split('?')[0];
            img.src = `${currentSrc}?t=${new Date().getTime()}`;
        });
    }, 100); // Refresh every 100 milliseconds (adjust as needed)
}



// Set up dragging functionality
function setupEventDelegation() {
    const cameraContainer = document.getElementById('cameraContainer');

    cameraContainer.addEventListener('mousedown', function(e) {
        if (e.target.classList.contains('resize-handle')) {
            resizeStart(e, e.target.parentElement);
        } else {
            const draggable = e.target.closest('.draggable');
            if (draggable) {
                dragStart(e, draggable);
            }
        }
    });

    window.addEventListener('mousemove', dragOrResize);
    window.addEventListener('mouseup', stopDragOrResize);
}



// Use event delegation for dragging
//const gridContainer = document.getElementById('gridContainer');
//
//gridContainer.addEventListener('mousedown', function(e) {
//    const draggable = e.target.closest('.draggable');
//    if (draggable) {
//        dragStart(e, draggable);
//    }
//});

window.addEventListener('mouseup', dragEnd);
window.addEventListener('mousemove', drag);

// Drag start
function dragStart(e, draggable) {
    console.log('Drag start');
    activeDraggable = draggable;
    const rect = activeDraggable.getBoundingClientRect();
    offsetX = e.clientX - rect.left;
    offsetY = e.clientY - rect.top;
    activeDraggable.style.cursor = 'grabbing';
    activeDraggable.style.zIndex = 1000; // Bring to front during dragging
    e.preventDefault();
}

function dragEnd(e) {
    if (activeDraggable) {
        const img = activeDraggable.querySelector('img');
        const ip = img.getAttribute('data-ip');
        if (!ip) {
            console.error("No IP found for dragged camera. Cannot update position.");
            activeDraggable.style.cursor = 'move';
            activeDraggable.style.zIndex = '';
            activeDraggable = null;
            return;
        }
        const position = {
            left: parseInt(activeDraggable.style.left, 10) || 0,
            top: parseInt(activeDraggable.style.top, 10) || 0
        };
        const size = {
            width: activeDraggable.offsetWidth || 320,
            height: activeDraggable.offsetHeight || 240
        };

        console.log("Updating camera with IP:", ip, position, size);

        if (ip && !isNaN(position.left) && !isNaN(position.top) && 
            !isNaN(size.width) && !isNaN(size.height)) {
            updateCamera(ip, position, size);
        } else {
            console.error("Invalid IP, position, or size data. Not updating camera.");
        }

        activeDraggable.style.cursor = 'move';
        activeDraggable.style.zIndex = '';
        activeDraggable = null;
    }
}

function drag(e) {
    if (activeDraggable) {
        console.log('Dragging');
        const container = activeDraggable.parentElement;
        const containerRect = container.getBoundingClientRect();

        let newX = e.clientX - offsetX - containerRect.left;
        let newY = e.clientY - offsetY - containerRect.top;

        // Optionally, constrain the draggable within the container
        newX = Math.max(0, Math.min(newX, container.clientWidth - activeDraggable.clientWidth));
        newY = Math.max(0, Math.min(newY, container.clientHeight - activeDraggable.clientHeight));

        activeDraggable.style.left = `${newX}px`;
        activeDraggable.style.top = `${newY}px`;

        e.preventDefault();
    }
}


// Update the form submission to refresh the camera list
document.getElementById('addCameraForm').addEventListener('submit', function(event) {
    event.preventDefault(); // Prevent default form submission

    const ipAddress = document.getElementById('ipAddress').value;
    console.log(`Attempting to add camera with IP: ${ipAddress}`);

    // Send the IP address via a POST request
    fetch('/add_camera', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/x-www-form-urlencoded',
        },
        body: new URLSearchParams({ ip_address: ipAddress })
    })
    .then(response => {
        if (response.ok) {
            console.log('Camera added successfully');
            // Fetch and render cameras without reloading
            fetchAndRenderCameras();
            fetchAndRenderCameraList(); // Refresh the camera list
        } else {
            console.error('Failed to add camera');
        }
    })
    .catch(error => {
        console.error('Error adding camera:', error);
    });
});

// Function to reload a single iframe
function reloadIframe(iframe) {
    iframe.src = iframe.src.split('?')[0] + '?t=' + new Date().getTime();
}

// Function to monitor iframes and reload on error
function monitorIframes() {
    const iframes = document.querySelectorAll('iframe.camera-stream');
    iframes.forEach(iframe => {
        iframe.onerror = function() {
            console.log(`Error detected in iframe ${iframe.id}, reloading...`);
            reloadIframe(iframe);
        };
    });
}

// Function to send position and size updates to the server
function updateCamera(ip, position, size) {
    fetch('/update_camera', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
        },
        body: JSON.stringify({
            ip: ip,
            position: position,
            size: size
        })
    })
    .then(response => {
        if (response.ok) {
            console.log('Camera position and size updated');
        } else {
            console.error('Failed to update camera position and size');
        }
    })
    .catch(error => {
        console.error('Error updating camera:', error);
    });
}

function monitorImages() {
    const images = document.querySelectorAll('img.camera-stream');
    images.forEach(img => {
        // Set up periodic refresh
        setInterval(() => {
            img.src = img.src.split('?')[0] + '?t=' + new Date().getTime();
        }, 10); // Refresh every 100 milliseconds
    });
}


// Function to fetch and render the camera list in the toolbar
async function fetchAndRenderCameraList() {
    try {
        const response = await fetch('/get_cameras');
        const cameras = await response.json();

        // Initialize recording states    &&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&ADDED&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&
        cameras.forEach(camera => {
            cameraRecordingStates[camera.ip] = camera.recording || false;
        });

        const cameraList = document.getElementById('cameraList');
        cameraList.innerHTML = ''; // Clear existing content

        for (const [index, camera] of cameras.entries()) {
            const listItem = document.createElement('li');
            listItem.classList.add('camera-item');
            listItem.dataset.index = index;

            const nameSpan = document.createElement('span');
            nameSpan.classList.add('camera-name');
            nameSpan.textContent = 'Loading...';

            const ipSpan = document.createElement('span');
            ipSpan.classList.add('camera-ip');
            ipSpan.textContent = camera.ip;

            if (!camera.visible) {
                nameSpan.classList.add('hidden-camera');
            }

            // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% Start Added stuff %%%%%%%%%%%%%%%%%%%%%%%%%%%%%
            // Create a container for the name and IP
            const nameIpContainer = document.createElement('div');
            nameIpContainer.classList.add('name-ip-container');
            nameIpContainer.appendChild(nameSpan);
            nameIpContainer.appendChild(ipSpan);

            // Create the record icon
            const recordIcon = document.createElement('img');
            recordIcon.src = '/static/images/record.png';
            recordIcon.classList.add('record-icon');
            recordIcon.dataset.ip = camera.ip;
            recordIcon.title = 'Start Recording';

            // Create the stop icon
            const stopIcon = document.createElement('img');
            stopIcon.src = '/static/images/stop.png';
            stopIcon.classList.add('stop-icon');
            stopIcon.dataset.ip = camera.ip;
            stopIcon.title = 'Stop Recording';

            // Determine initial visibility of icons based on recording state
            const isRecording = cameraRecordingStates[camera.ip] || false;
            recordIcon.style.display = isRecording ? 'none' : 'inline-block';
            stopIcon.style.display = isRecording ? 'inline-block' : 'none';

            // Add event listeners for the icons
            recordIcon.addEventListener('click', function (event) {
                event.stopPropagation();
                startRecording(camera.ip, recordIcon, stopIcon);
            });

            stopIcon.addEventListener('click', function (event) {
                event.stopPropagation();
                stopRecording(camera.ip, recordIcon, stopIcon);
            });

            // Append elements to the list item
            listItem.appendChild(nameIpContainer);
            listItem.appendChild(recordIcon);
            listItem.appendChild(stopIcon);
            // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% END Added stuff %%%%%%%%%%%%%%%%%%%%%%%%%%%%%

            const removeButton = document.createElement('button');
            removeButton.classList.add('remove-camera');
            removeButton.textContent = 'X';
            removeButton.dataset.ip = camera.ip;

            //listItem.appendChild(nameSpan);
            //listItem.appendChild(document.createElement('br'));
            //listItem.appendChild(ipSpan);
            listItem.appendChild(removeButton);
            cameraList.appendChild(listItem);

            removeButton.addEventListener('click', function () {
                confirmCameraRemoval(camera.ip);
            });

            listItem.addEventListener('click', function () {
                toggleCameraVisibility(index);
            });

            try {
                const settingsResponse = await fetch(`/camera_settings/${camera.ip}`);
                const settings = await settingsResponse.json();
                const cameraName = settings.theatreChatName || 'Unnamed Camera';
                nameSpan.textContent = cameraName;
            } catch (error) {
                console.error(`Error fetching settings for camera ${camera.ip}:`, error);
                nameSpan.textContent = 'Unnamed Camera';
            }
        }
    } catch (error) {
        console.error('Error fetching camera list:', error);
    }
}


//&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&ADDED FUNCTIONS&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&
// Add the startRecording function
function startRecording(ip_address, recordIcon, stopIcon) {
    fetch(`/start_recording/${ip_address}`)
        .then(response => response.json())
        .then(data => {
            //alert(data.status);
            // Update recording state
            cameraRecordingStates[ip_address] = true;
            // Swap the visibility of the icons
            recordIcon.style.display = 'none';
            stopIcon.style.display = 'inline-block';
        })
        .catch(error => {
            console.error(`Error starting recording for camera ${ip_address}:`, error);
        });
}

// Add the stopRecording function
function stopRecording(ip_address, recordIcon, stopIcon) {
    fetch(`/stop_recording/${ip_address}`)
        .then(response => response.json())
        .then(data => {
            //alert(data.status);
            // Update recording state
            cameraRecordingStates[ip_address] = false;
            // Swap the visibility of the icons
            recordIcon.style.display = 'inline-block';
            stopIcon.style.display = 'none';
        })
        .catch(error => {
            console.error(`Error stopping recording for camera ${ip_address}:`, error);
        });
}
//&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&END ADDED FUNCTIONS&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&





function confirmCameraRemoval(ip) {
    //const confirmation = confirm("Are you sure you want to delete this camera?");
    //if (confirmation) {
        removeCamera(ip); // Call the function to remove the camera if confirmed
    //}
}

// Function to toggle camera visibility based on its index
function toggleCameraVisibility(index) {
    const cameraDiv = document.getElementById(`draggable_${index}`);
    const cameraListItem = document.querySelector(`[data-index="${index}"] .camera-name`);
    
    let isVisible;
    if (cameraDiv) {
        if (cameraDiv.style.display === 'none') {
            cameraDiv.style.display = 'block';
            cameraListItem.classList.remove('hidden-camera'); // Remove strikethrough
            isVisible = true;
        } else {
            cameraDiv.style.display = 'none';
            cameraListItem.classList.add('hidden-camera'); // Add strikethrough
            isVisible = false;
        }

        // Update the camera visibility in the current scene
        updateCameraVisibility(index, isVisible);
    }
}


// Function to update camera visibility in the scene and save
function updateCameraVisibility(index, isVisible) {
    // Get the IP address of the camera
    const camera = document.getElementById(`camera_${index}`);
    const ip = camera.src.split('/camera_stream/')[1];

    // Send the updated visibility to the server
    fetch('/update_camera_visibility', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
        },
        body: JSON.stringify({
            ip: ip,
            visible: isVisible
        })
    })
    .then(response => {
        if (response.ok) {
            console.log('Camera visibility updated');
        } else {
            console.error('Failed to update camera visibility');
        }
    })
    .catch(error => {
        console.error('Error updating camera visibility:', error);
    });
}



// Function to remove a camera
function removeCamera(ip) {
    fetch(`/remove_camera/${ip}`)
        .then(response => {
            if (response.ok) {
                console.log(`Camera ${ip} removed successfully`);
                // Refresh the camera list and grid
                fetchAndRenderCameraList();
                fetchAndRenderCameras();
            } else {
                console.error(`Failed to remove camera ${ip}`);
            }
        })
        .catch(error => {
            console.error('Error removing camera:', error);
        });
}

function setupResizeFunctionality() {
    const cameraContainer = document.getElementById('cameraContainer');

    cameraContainer.addEventListener('mousedown', function(e) {
        if (e.target.classList.contains('resize-handle')) {
            resizeStart(e, e.target.parentElement);
        }
    });

    window.addEventListener('mousemove', resize);
    window.addEventListener('mouseup', resizeEnd);
}

// Resize start
function resizeStart(e, draggable) {
    console.log('Resize start');
    isResizing = true;
    activeDraggable = draggable;
    initialWidth = activeDraggable.offsetWidth;
    initialHeight = activeDraggable.offsetHeight;
    initialMouseX = e.clientX;
    initialMouseY = e.clientY;
    activeDraggable.style.zIndex = 1000; // Bring to front during resizing
    e.preventDefault();
}

function resize(e) {
    if (isResizing && activeDraggable) {
        const dx = e.clientX - initialMouseX;
        const dy = e.clientY - initialMouseY;

        let newWidth = initialWidth + dx;
        let newHeight = (newWidth * 3) / 4; // Maintain 4:3 aspect ratio

        // Set minimum size limits
        newWidth = Math.max(100, newWidth);
        newHeight = Math.max(75, newHeight);

        activeDraggable.style.width = `${newWidth}px`;
        activeDraggable.style.height = `${newHeight}px`;

        e.preventDefault();
    }
}

function resizeEnd(e) {
    if (isResizing) {
        console.log('Resize end');
        activeDraggable.style.zIndex = ''; // Reset z-index after resizing

        // Save the updated size
        const ip = activeDraggable.querySelector('img').src.split('/camera_stream/')[1];
        const position = {
            left: parseInt(activeDraggable.style.left, 10),
            top: parseInt(activeDraggable.style.top, 10)
        };

        const size = {
            width: activeDraggable.offsetWidth,
            height: activeDraggable.offsetHeight
        };

        // Send updated position and size to the server
        updateCamera(ip, position, size);

        isResizing = false;
        activeDraggable = null;
    }
}

// Handle drag or resize
function dragOrResize(e) {
    if (activeDraggable && !isResizing) {
        // Dragging
        console.log('Dragging');
        const newX = e.clientX - offsetX;
        const newY = e.clientY - offsetY;

        // Update position
        activeDraggable.style.left = `${newX}px`;
        activeDraggable.style.top = `${newY}px`;

    } else if (isResizing && activeDraggable) {
        // Resizing
        console.log('Resizing');
        const dx = e.clientX - initialMouseX;
        const dy = e.clientY - initialMouseY;

        let newWidth = initialWidth + dx;
        let newHeight = (newWidth * 3) / 4; // Maintain 4:3 aspect ratio

        // Set minimum size limits
        newWidth = Math.max(100, newWidth);
        newHeight = Math.max(75, newHeight);

        activeDraggable.style.width = `${newWidth}px`;
        activeDraggable.style.height = `${newHeight}px`;
    }
    e.preventDefault();
}

// Stop drag or resize
function stopDragOrResize(e) {
    if (activeDraggable) {
        console.log('Drag or resize end');
        activeDraggable.style.cursor = 'move';
        activeDraggable.style.zIndex = ''; // Reset z-index after dragging or resizing
        activeDraggable = null;
    }
    isResizing = false; // Stop resizing
}








// Fetch camera settings for label overlay
function fetchCameraSettings(ip, overlay, imgElement) {
    // Check if a label already exists to prevent duplicate overlays
    const existingLabel = overlay.querySelector('.camera-label');
    if (existingLabel) {
        console.log(`Overlay for IP: ${ip} already has camera settings. Skipping duplicates.`);
        return; // If we already have camera settings, do not recreate them
    }

    fetch(`/camera_settings/${ip}`)
        .then(response => response.json())
        .then(settings => {
            const theatreChatName = settings.theatreChatName || 'Unnamed Camera';

            // Create or update the label for the camera
            const label = document.createElement('a');
            label.classList.add('camera-label');
            label.textContent = theatreChatName;
            label.href = `http://${ip}/`;
            label.target = '_blank';
            label.style.textDecoration = 'none';
            label.style.color = '#fff';
            overlay.appendChild(label);

            // Create and add a refresh button
            const refreshButton = document.createElement('img');
            refreshButton.src = '/static/images/refresh.png';
            refreshButton.classList.add('refresh-icon');
            refreshButton.alt = 'Refresh';
            refreshButton.style.cursor = 'pointer';
            console.log(`Refresh button created for IP: ${ip}`);

            refreshButton.addEventListener('click', (event) => {
                event.stopPropagation();
                console.log(`Refresh button clicked for IP: ${ip}`);
            
                if (imgElement) {
                    const src = imgElement.src.split('?')[0];
                    // Only add ?t= if the URL starts with http or https
                    if (src.startsWith('http') || src.startsWith('/camera_stream')) {
                        imgElement.src = `${src}?t=${new Date().getTime()}`;
                    } else {
                        // If it's a data URI, just reassigning the same src would "refresh"
                        imgElement.src = src; 
                    }
                    console.log(`Camera stream refreshed for IP ${ip}`);
                } else {
                    console.error(`Image element for IP ${ip} not found`);
                }
            });
            

            overlay.appendChild(refreshButton);
            console.log(`Refresh button appended to overlay for IP: ${ip}`);

            // Fetch battery status and add battery icon
            fetch(`/get_battery_percentage/${ip}`)
                .then(response => response.text())
                .then(batteryStatus => {
                    console.log(`Battery status for ${ip}: ${batteryStatus}`);
                    
                    // Create the battery icon
                    const batteryIcon = document.createElement('img');
                    batteryIcon.classList.add('battery-icon');

                    // Determine the appropriate battery icon based on status
                    if (batteryStatus === 'N/A') {
                        batteryIcon.src = '/static/images/battery_charging.png';
                    } else {
                        const batteryPercentage = parseInt(batteryStatus, 10);
                        if (batteryPercentage >= 90) {
                            batteryIcon.src = '/static/images/battery_100.png';
                        } else if (batteryPercentage >= 65) {
                            batteryIcon.src = '/static/images/battery_75.png';
                        } else if (batteryPercentage >= 40) {
                            batteryIcon.src = '/static/images/battery_50.png';
                        } else if (batteryPercentage >= 20) {
                            batteryIcon.src = '/static/images/battery_25.png';
                        } else {
                            batteryIcon.src = '/static/images/battery_empty.png';
                        }
                    }

                    overlay.appendChild(batteryIcon);
                })
                .catch(error => {
                    console.error(`Error fetching battery status from camera ${ip}:`, error);
                });
        })
        .catch(error => {
            console.error(`Error fetching settings from camera ${ip}:`, error);

            // Fallback if settings fetch fails and we didn't have an overlay yet
            const label = document.createElement('a');
            label.classList.add('camera-label');
            label.textContent = 'Camera';
            label.href = `http://${ip}/`;
            label.target = '_blank';
            label.style.textDecoration = 'none';
            label.style.color = '#fff';

            overlay.appendChild(label);
        });
}




let currentScene = {};

// Save the current layout as a scene
function saveScene() {
    const sceneNumber = prompt("Enter scene number:");
    const sceneName = prompt("Enter scene name:") || `Scene ${sceneNumber}`;

    const sceneData = {
        sceneNumber: parseInt(sceneNumber),
        sceneName: sceneName,
        cameras: []
    };

    const cameras = document.querySelectorAll('.draggable');
    cameras.forEach(camera => {
        const img = camera.querySelector('img');
        const ip = img.getAttribute('data-ip'); // Use data-ip attribute for IP

        sceneData.cameras.push({
            ip: ip || "0.0.0.0",  // Fallback IP
            name: camera.querySelector('.camera-label')?.textContent || "Unnamed Camera", // Fetch camera name
            position: {
                left: parseInt(camera.style.left) || 0,
                top: parseInt(camera.style.top) || 0
            },
            size: {
                width: camera.offsetWidth || 320,
                height: camera.offsetHeight || 240
            },
            visible: camera.style.display !== 'none'
        });
    });

    fetch('/save_scene', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json'
        },
        body: JSON.stringify(sceneData)
    }).then(response => response.json())
      .then(data => {
          if (data.status === "success") {
              alert("Scene saved successfully");
          }
      });
}


// Load a scene
function loadScene() {
    const sceneNumber = prompt("Enter scene number to load:");
    
    fetch(`/load_scene/${sceneNumber}`)
        .then(response => response.json())
        .then(scene => {
            if (scene.error) {
                alert(scene.error);
                return;
            }

            // Clear current layout
            document.getElementById('cameraContainer').innerHTML = '';

            // Rebuild camera layout based on the scene
            scene.cameras.forEach((camera, index) => {
                const draggable = document.createElement('div');
                draggable.classList.add('draggable');
                draggable.id = `draggable_${index}`;

                // Apply visibility from the scene
                if (!camera.visible) {
                    draggable.style.display = 'none';  // Hide the camera if it's not visible
                } else {
                    draggable.style.display = 'block'; // Ensure it's visible if it's marked as visible
                }

                draggable.style.left = `${camera.position.left}px`;
                draggable.style.top = `${camera.position.top}px`;
                draggable.style.width = `${camera.size.width}px`;
                draggable.style.height = `${camera.size.height}px`;

                const img = document.createElement('img');
                img.id = `camera_${index}`;
                img.src = `/camera_stream/${camera.ip}`;
                img.classList.add('camera-stream');
                
                draggable.appendChild(img);

                // Append to camera container
                document.getElementById('cameraContainer').appendChild(draggable);
            });

            // Now update the camera list in the toolbar
            updateCameraList(scene.cameras);

            // Update current scene label
            updateCurrentSceneLabel(scene.sceneNumber, scene.sceneName);
        });
}


// Function to update the camera list when loading a scene
async function updateCameraList(cameras) {
    const cameraList = document.getElementById('cameraList');
    cameraList.innerHTML = ''; // Clear the current list

    // Fetch the current cameras with recording states
    let camerasWithRecording = [];
    try {
        const response = await fetch('/get_cameras');
        camerasWithRecording = await response.json();
    } catch (error) {
        console.error('Error fetching cameras with recording states:', error);
    }

    // Create a mapping from IP to recording state
    const recordingStates = {};
    camerasWithRecording.forEach(cam => {
        recordingStates[cam.ip] = cam.recording || false;
        // Update cameraRecordingStates
        cameraRecordingStates[cam.ip] = cam.recording || false;
    });

    cameras.forEach((camera, index) => {
        const listItem = document.createElement('li');
        listItem.classList.add('camera-item');
        listItem.dataset.index = index;

        // Create the name span with default text
        const nameSpan = document.createElement('span');
        nameSpan.classList.add('camera-name');
        nameSpan.textContent = 'Loading...';

        const ipSpan = document.createElement('span');
        ipSpan.classList.add('camera-ip');
        ipSpan.textContent = camera.ip;

        // Add strikethrough if the camera is hidden
        if (!camera.visible) {
            nameSpan.classList.add('hidden-camera');
        }

        // Create a container for the name and IP
        const nameIpContainer = document.createElement('div');
        nameIpContainer.classList.add('name-ip-container');
        nameIpContainer.appendChild(nameSpan);
        nameIpContainer.appendChild(ipSpan);

        // Create the record icon
        const recordIcon = document.createElement('img');
        recordIcon.src = '/static/images/record.png';
        recordIcon.classList.add('record-icon');
        recordIcon.dataset.ip = camera.ip;
        recordIcon.title = 'Start Recording';

        // Create the stop icon
        const stopIcon = document.createElement('img');
        stopIcon.src = '/static/images/stop.png';
        stopIcon.classList.add('stop-icon');
        stopIcon.dataset.ip = camera.ip;
        stopIcon.title = 'Stop Recording';

        // Determine initial visibility of icons based on recording state
        const isRecording = recordingStates[camera.ip] || false;
        recordIcon.style.display = isRecording ? 'none' : 'inline-block';
        stopIcon.style.display = isRecording ? 'inline-block' : 'none';

        // Add event listeners for the icons
        recordIcon.addEventListener('click', function (event) {
            event.stopPropagation();
            startRecording(camera.ip, recordIcon, stopIcon);
        });

        stopIcon.addEventListener('click', function (event) {
            event.stopPropagation();
            stopRecording(camera.ip, recordIcon, stopIcon);
        });

        // Append elements to the list item
        listItem.appendChild(nameIpContainer);
        listItem.appendChild(recordIcon);
        listItem.appendChild(stopIcon);

        // Create remove button
        const removeButton = document.createElement('button');
        removeButton.classList.add('remove-camera');
        removeButton.textContent = 'X';
        removeButton.dataset.ip = camera.ip;

        // Append the remove button to the list item
        listItem.appendChild(removeButton);

        // Append the list item to the camera list
        cameraList.appendChild(listItem);

        // Attach event listener for remove button with confirmation
        removeButton.addEventListener('click', function () {
            confirmCameraRemoval(camera.ip);
        });

        // Attach event listener for toggling camera visibility
        listItem.addEventListener('click', function () {
            toggleCameraVisibility(index);
        });

        // Fetch the camera settings to get the name
        fetch(`/camera_settings/${camera.ip}`)
            .then(response => response.json())
            .then(settings => {
                const cameraName = settings.theatreChatName || 'Unnamed Camera';
                nameSpan.textContent = cameraName;
            })
            .catch(error => {
                console.error(`Error fetching settings for camera ${camera.ip}:`, error);
                nameSpan.textContent = 'Unnamed Camera';
            });
    });
}







// Delete a scene
function deleteScene() {
    const sceneNumber = prompt("Enter scene number to delete:");

    if (confirm(`Are you sure you want to delete scene ${sceneNumber}?`)) {
        fetch(`/delete_scene/${sceneNumber}`, { method: 'DELETE' })
            .then(response => response.json())
            .then(data => {
                if (data.status === "success") {
                    alert("Scene deleted successfully");
                }
            });
    }
}

// Update the label in the top-right corner to show the current scene
function updateCurrentSceneLabel(sceneNumber, sceneName) {
    const sceneLabel = document.getElementById('sceneLabel');
    sceneLabel.textContent = `Scene ${sceneNumber}: ${sceneName}`;
}

// Add buttons to save, load, and delete scenes
document.getElementById('saveSceneButton').addEventListener('click', saveScene);
document.getElementById('loadSceneButton').addEventListener('click', loadScene);
document.getElementById('deleteSceneButton').addEventListener('click', deleteScene);


let lastLoadedScene = null;


function refreshSceneLayout(sceneData) {
    console.log("Refreshing scene layout with scene data:", sceneData);

    // Clear the current grid
    const cameraContainer = document.getElementById('cameraContainer');
    cameraContainer.innerHTML = '';

    // Rebuild camera layout based on the scene
    sceneData.cameras.forEach((camera, index) => {
        const draggable = document.createElement('div');
        draggable.classList.add('draggable');
        draggable.id = `draggable_${index}`;

        // Apply visibility from the scene
        draggable.style.display = camera.visible ? 'block' : 'none';
        draggable.style.left = `${camera.position.left}px`;
        draggable.style.top = `${camera.position.top}px`;
        draggable.style.width = `${camera.size.width}px`;
        draggable.style.height = `${camera.size.height}px`;

        const img = document.createElement('img');
        img.id = `camera_${index}`;
        img.src = `/camera_stream/${camera.ip}`;
        img.classList.add('camera-stream');

        img.setAttribute('data-ip', camera.ip);

        // Create overlay and label (if needed)
        const overlay = document.createElement('div');
        overlay.classList.add('overlay');

        // Fetch camera settings for the label (optional)
        fetchCameraSettings(camera.ip, overlay, img);

        // Append elements
        draggable.appendChild(img);
        draggable.appendChild(overlay);

        // **Add a resize handle for resizing functionality**
        const resizeHandle = document.createElement('div');
        resizeHandle.classList.add('resize-handle');
        draggable.appendChild(resizeHandle);

        cameraContainer.appendChild(draggable);
    });

    // Update the camera list in the toolbar (if applicable)
    updateCameraList(sceneData.cameras);

    // Update the current scene label
    updateCurrentSceneLabel(sceneData.sceneNumber, sceneData.sceneName);

    // **Re-attach event listeners for drag and resize**
    setupEventDelegation();
    setupResizeFunctionality();
}




// Make scene label clickable
document.getElementById('sceneLabel').addEventListener('click', function() {
    showSceneSelectionModal();
});

// Function to show the scene selection modal
function showSceneSelectionModal() {
    // Fetch scenes from the server
    fetch('/get_scenes')
        .then(response => response.json())
        .then(scenes => {
            // Sort scenes by sceneNumber
            scenes.sort((a, b) => a.sceneNumber - b.sceneNumber);
            
            // Get the sceneList element
            const sceneList = document.getElementById('sceneList');
            sceneList.innerHTML = ''; // Clear existing content
            
            // Populate the scene list
            scenes.forEach(scene => {
                const listItem = document.createElement('li');
                listItem.textContent = `Scene ${scene.sceneNumber}: ${scene.sceneName}`;
                listItem.dataset.sceneNumber = scene.sceneNumber;
                sceneList.appendChild(listItem);
                
                // Add click event to load the scene
                listItem.addEventListener('click', function() {
                    loadScene(scene.sceneNumber);
                    closeSceneSelectionModal();
                });
            });
            
            // Show the modal
            document.getElementById('sceneSelectionModal').style.display = 'block';
        })
        .catch(error => {
            console.error('Error fetching scenes:', error);
        });
}



// Function to close the modal
function closeSceneSelectionModal() {
    document.getElementById('sceneSelectionModal').style.display = 'none';
}




function loadScene(sceneNumber) {
    fetch(`/load_scene/${sceneNumber}`)
        .then(response => response.json())
        .then(scene => {
            if (scene.error) {
                alert(scene.error);
                return;
            }

            // Clear current layout
            document.getElementById('cameraContainer').innerHTML = '';

            // Rebuild camera layout based on the scene
            scene.cameras.forEach((camera, index) => {
                renderCamera(camera, index);
            });

            // Update the camera list in the toolbar
            updateCameraList(scene.cameras);

            // Update current scene label
            updateCurrentSceneLabel(scene.sceneNumber, scene.sceneName);

            // Re-attach event listeners
            setupEventDelegation();
            setupResizeFunctionality();
        })
        .catch(error => {
            console.error('Error loading scene:', error);
        });
}

socket.on('camera_discovered', function(data) {
    // data.ip is the newly discovered camera's IP
    console.log('Received camera_discovered event:', data);
    showDiscoveredCameraNotification(data.ip);
});

function showDiscoveredCameraNotification(ip) {
    const container = document.getElementById('mdnsNotifications');
    const notification = document.createElement('div');
    notification.classList.add('notification');
    notification.innerHTML = `
        Discovered new camera at IP ${ip}.
        <button class="addCameraBtn">Add</button>
    `;

    const addButton = notification.querySelector('.addCameraBtn');
    addButton.addEventListener('click', () => {
        // Call your existing add_camera endpoint
        fetch('/add_camera', {
            method: 'POST',
            headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
            body: new URLSearchParams({ ip_address: ip })
        }).then(resp => {
            if (resp.ok) {
                // Remove notification
                container.removeChild(notification);
                // Optionally refresh your camera list
                fetchAndRenderCameras();
                fetchAndRenderCameraList();
            } else {
                console.error('Failed to add camera.');
            }
        });
    });

    // Optionally auto-close the notification after N seconds
    setTimeout(() => {
        if (container.contains(notification)) {
            container.removeChild(notification);
        }
    }, 15000);

    container.appendChild(notification);
}



socket.on('battery_status', function(data) {
    const ip = data.ip;
    const batteryPercentage = data.battery_percentage;

    // Update the battery icon for the corresponding camera
    updateBatteryStatus(ip, batteryPercentage);
});

function updateBatteryStatus(ip, batteryPercentage) {
    // Find the overlay element for the camera with the given IP
    const overlays = document.querySelectorAll('.overlay');
    overlays.forEach(overlay => {
        const label = overlay.querySelector('.camera-label');
        if (label && label.href.includes(ip)) {
            // Find the battery icon within this overlay
            let batteryIcon = overlay.querySelector('.battery-icon');
            if (!batteryIcon) {
                // Create the battery icon if it doesn't exist
                batteryIcon = document.createElement('img');
                batteryIcon.classList.add('battery-icon');
                overlay.appendChild(batteryIcon);
            }

            // Update the battery icon based on the battery percentage
            if (batteryPercentage === 'N/A') {
                batteryIcon.src = '/static/images/battery_charging.png';
            } else {
                const percentage = parseInt(batteryPercentage, 10);
                if (percentage >= 90) {
                    batteryIcon.src = '/static/images/battery_100.png';
                } else if (percentage >= 65) {
                    batteryIcon.src = '/static/images/battery_75.png';
                } else if (percentage >= 40) {
                    batteryIcon.src = '/static/images/battery_50.png';
                } else if (percentage >= 20) {
                    batteryIcon.src = '/static/images/battery_25.png';
                } else {
                    batteryIcon.src = '/static/images/battery_empty.png';
                }
            }
        }
    });
}

socket.on('camera_settings', function(data) {
    const ip = data.ip;
    const settings = data.settings;

    // Update the camera label or other settings
    updateCameraSettings(ip, settings);
});

function updateCameraSettings(ip, settings) {
    const overlays = document.querySelectorAll('.overlay');
    overlays.forEach(overlay => {
        const label = overlay.querySelector('.camera-label');
        if (label && label.href.includes(ip)) {
            // Update the label text
            label.textContent = settings.theatreChatName || 'Unnamed Camera';
        }
    });
}

socket.on('scene_loaded', function(scene) {
    // Update the UI with the new scene data
    refreshSceneLayout(scene);
});

socket.on('frame', function(data) {
    const ip = data.ip;
    const frameData = data.frame;  // Base64 encoded frame

    // Update the image element corresponding to this IP
    updateCameraFrame(ip, frameData);
});

let lastFrameTime = {}; // Store the last update time for each camera

// Function to update the camera frame
function updateCameraFrame(ip, frameData) {
    const now = Date.now();
    if (!lastFrameTime[ip] || now - lastFrameTime[ip] >= 100) { // Update every 100ms
        lastFrameTime[ip] = now;
        const imgElements = document.querySelectorAll(`img.camera-stream[data-ip='${ip}']`);
        imgElements.forEach(img => {
            if (!img.classList.contains('record-icon') && !img.classList.contains('stop-icon')) {
                img.src = `data:image/jpeg;base64,${frameData}`;
            }
        });
    }
}


// Call pollForSceneUpdates on page load to start polling
window.onload = function() {
    fetchAndRenderCameras();
    fetchAndRenderCameraList();
    setupEventDelegation();
    setupResizeFunctionality();
    //pollForSceneUpdates();  // Start polling for scene changes every 5 seconds

    // Auto-hiding toolbar functionality
    const toolbar = document.getElementById('toolbar');
    const hoverArea = document.querySelector('.toolbar-hover-area');
    const lockIcon = document.getElementById('lockIcon');
    const unlockIcon = document.getElementById('unlockIcon');
    

    hoverArea.addEventListener('mouseenter', () => {
        toolbar.classList.add('visible');
    });

    toolbar.addEventListener('mouseleave', () => {
        if (!toolbarLocked) {
            toolbar.classList.remove('visible');
        }
    });

    lockIcon.addEventListener('click', () => {
        // Lock the toolbar
        toolbarLocked = true;
        toolbar.classList.add('visible'); // Ensure it's visible
        lockIcon.style.display = 'none';
        unlockIcon.style.display = 'inline-block';
    });
    
    unlockIcon.addEventListener('click', () => {
        // Unlock the toolbar
        toolbarLocked = false;
        unlockIcon.style.display = 'none';
        lockIcon.style.display = 'inline-block';
        // If the mouse is not currently hovering over the toolbar or hover area, hide it
        // (Optional check could be added here if desired)
    });


    document.getElementById('sceneLabel').addEventListener('click', function() {
        showSceneSelectionModal();
    });

    // Close button event listener
    document.getElementById('closeSceneModal').addEventListener('click', function() {
        closeSceneSelectionModal();
    });

    // Close modal when clicking outside the modal content
    window.addEventListener('click', function(event) {
        const modal = document.getElementById('sceneSelectionModal');
        if (event.target == modal) {
            closeSceneSelectionModal();
        }
    });
};

socket.on('connect', () => {
    console.log('Socket.IO client connected');
});

socket.on('connect_error', (error) => {
    console.error('Socket.IO connection error:', error);
});

socket.on('error', (error) => {
    console.error('Socket.IO error:', error);
});

socket.on('disconnect', () => {
    console.warn('Socket.IO client disconnected');
});
