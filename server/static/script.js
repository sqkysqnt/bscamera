// script.js

let activeDraggable = null;
let offsetX, offsetY;
let isResizing = false;
let initialWidth, initialHeight;
let initialMouseX, initialMouseY;


// Adjust initial positions as needed
//const left = (index % 3) * 200; // Example value
//const top = Math.floor(index / 3) * 150; // Example value


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


function renderCamera(camera, index) {
    const cameraContainer = document.getElementById('cameraContainer');

    const draggable = document.createElement('div');
    draggable.classList.add('draggable');
    draggable.id = `draggable_${index}`;

    if (!camera.visible) draggable.style.display = 'none';

    // Set position and size
    draggable.style.left = `${camera.position.left || (index % 3) * 350}px`;
    draggable.style.top = `${camera.position.top || Math.floor(index / 3) * 300}px`;
    draggable.style.width = `${camera.size.width || 320}px`;
    draggable.style.height = `${camera.size.height || 240}px`;

    // Create the image element for MJPEG stream
    const img = document.createElement('img');
    img.id = `camera_${index}`;
    img.src = `/camera_stream/${camera.ip}`;
    img.classList.add('camera-stream');

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
    overlay.appendChild(label);

    // Append the image, overlay to the draggable div
    draggable.appendChild(img);
    draggable.appendChild(overlay);

    // Append the draggable element to the camera container
    cameraContainer.appendChild(draggable);

    // Add a resize handle for resizing functionality
    const resizeHandle = document.createElement('div');
    resizeHandle.classList.add('resize-handle');
    draggable.appendChild(resizeHandle);
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
        console.log('Drag end');
        activeDraggable.style.cursor = 'move';
        activeDraggable.style.zIndex = ''; // Reset z-index after dragging

        // Save the updated position
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

            const removeButton = document.createElement('button');
            removeButton.classList.add('remove-camera');
            removeButton.textContent = 'X';
            removeButton.dataset.ip = camera.ip;

            listItem.appendChild(nameSpan);
            listItem.appendChild(document.createElement('br'));
            listItem.appendChild(ipSpan);
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




function confirmCameraRemoval(ip) {
    const confirmation = confirm("Are you sure you want to delete this camera?");
    if (confirmation) {
        removeCamera(ip); // Call the function to remove the camera if confirmed
    }
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
// Fetch camera settings for label overlay
function fetchCameraSettings(ip, overlay) {
    fetch(`/camera_settings/${ip}`)
        .then(response => response.json())
        .then(settings => {
            const theatreChatName = settings.theatreChatName || 'Unnamed Camera';

            // Create a clickable link element for the label
            const label = document.createElement('a');
            label.classList.add('camera-label');
            label.textContent = theatreChatName;
            label.href = `http://${ip}/`;  // Set the link to the camera's IP address
            label.target = '_blank';  // Open in a new tab
            label.style.textDecoration = 'none';  // Remove underline for link
            label.style.color = '#fff';  // Ensure text is white

            // Ensure the label can be clicked without interference
            label.style.pointerEvents = 'auto'; // Enable pointer events

            // Append the clickable label to the overlay
            overlay.appendChild(label);
        })
        .catch(error => {
            console.error(`Error fetching settings from camera ${ip}:`, error);

            // Handle errors by displaying a default name as a clickable link
            const label = document.createElement('a');
            label.classList.add('camera-label');
            label.textContent = 'Camera';
            label.href = `http://${ip}/`;
            label.target = '_blank';
            label.style.textDecoration = 'none';
            label.style.color = '#fff';
            label.style.pointerEvents = 'auto'; // Enable pointer events

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
        const ip = img.src.split('/camera_stream/')[1];

        sceneData.cameras.push({
            ip: ip,
            position: {
                left: parseInt(camera.style.left),
                top: parseInt(camera.style.top)
            },
            size: {
                width: camera.offsetWidth,
                height: camera.offsetHeight
            },
            visible: !camera.classList.contains('hidden')
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
function updateCameraList(cameras) {
    const cameraList = document.getElementById('cameraList');
    cameraList.innerHTML = ''; // Clear the current list

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

        // Add strikethrough and italic if the camera is hidden
        if (!camera.visible) {
            nameSpan.classList.add('hidden-camera');  // Add strikethrough and italic if hidden
        }

        // Create remove button
        const removeButton = document.createElement('button');
        removeButton.classList.add('remove-camera');
        removeButton.textContent = 'X';
        removeButton.dataset.ip = camera.ip;

        // Align the name and IP with spacing between them
        const nameIpContainer = document.createElement('div');
        nameIpContainer.classList.add('name-ip-container');
        nameIpContainer.appendChild(nameSpan);
        nameIpContainer.appendChild(ipSpan);

        listItem.appendChild(nameIpContainer);
        listItem.appendChild(removeButton);
        cameraList.appendChild(listItem);

        // Attach event listener for remove button with confirmation
        removeButton.addEventListener('click', function() {
            confirmCameraRemoval(camera.ip);
        });

        // Attach event listener for toggling camera visibility
        listItem.addEventListener('click', function() {
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

function pollForSceneUpdates() {
    setInterval(() => {
        fetch('/get_last_scene')  // Request the last scene from the server
            .then(response => response.json())
            .then(scene => {
                if (!scene.error) {
                    // Update the camera layout with the new scene data
                    refreshSceneLayout(scene);
                }
            })
            .catch(error => console.error('Error fetching last scene:', error));
    }, 5000);  // Poll every 5 seconds
}

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

        // Create overlay and label (if needed)
        const overlay = document.createElement('div');
        overlay.classList.add('overlay');

        // Fetch camera settings for the label (optional)
        fetchCameraSettings(camera.ip, overlay);

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





// Call pollForSceneUpdates on page load to start polling
window.onload = function() {
    fetchAndRenderCameras();
    fetchAndRenderCameraList();
    setupEventDelegation();
    setupResizeFunctionality();
    pollForSceneUpdates();  // Start polling for scene changes every 5 seconds

    // Auto-hiding toolbar functionality
    const toolbar = document.getElementById('toolbar');
    const hoverArea = document.querySelector('.toolbar-hover-area');

    hoverArea.addEventListener('mouseenter', () => {
        toolbar.classList.add('visible');
    });

    toolbar.addEventListener('mouseleave', () => {
        toolbar.classList.remove('visible');
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
