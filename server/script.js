// script.js

let activeDraggable = null;
let offsetX, offsetY;

// Function to fetch cameras and render them
function fetchAndRenderCameras() {
    fetch('/get_cameras')
    .then(response => response.json())
    .then(cameras => {
        const gridContainer = document.getElementById('gridContainer');
        gridContainer.innerHTML = ''; // Clear existing content

        cameras.forEach((ip, index) => {
            const left = (index % 3) * 640;
            const top = Math.floor(index / 3) * 480;

            const draggable = document.createElement('div');
            draggable.classList.add('draggable');
            draggable.id = `draggable_${index}`;
            draggable.style.left = `${left}px`;
            draggable.style.top = `${top}px`;

            const iframe = document.createElement('iframe');
            iframe.id = `camera_${index}`;
            iframe.src = `/camera_stream/${ip}`;
            iframe.classList.add('camera-stream');
            iframe.width = '640';
            iframe.height = '480';
            iframe.frameBorder = '0';
            iframe.allowFullscreen = true;

            draggable.appendChild(iframe);
            gridContainer.appendChild(draggable);
        });

        // Re-initialize draggable elements after rendering
        initializeDraggables();

        // Monitor iframes for errors and reload
        monitorIframes();
    })
    .catch(error => {
        console.error('Error fetching cameras:', error);
    });
}


// Initialize draggable elements
function initializeDraggables() {
    const draggables = document.querySelectorAll('.draggable');

    draggables.forEach(draggable => {
        draggable.addEventListener('mousedown', dragStart);
    });
}

window.addEventListener('mouseup', dragEnd);
window.addEventListener('mousemove', drag);

function dragStart(e) {
    activeDraggable = e.currentTarget;
    offsetX = e.clientX - activeDraggable.getBoundingClientRect().left;
    offsetY = e.clientY - activeDraggable.getBoundingClientRect().top;
    activeDraggable.style.cursor = 'grabbing';
}

function dragEnd(e) {
    if (activeDraggable) {
        activeDraggable.style.cursor = 'move';
        activeDraggable = null;
    }
}

function drag(e) {
    if (activeDraggable) {
        const gridSizeX = 640; // Grid cell width
        const gridSizeY = 480; // Grid cell height

        // Calculate the new position, ensuring alignment with the grid
        const newX = Math.round((e.clientX - offsetX) / gridSizeX) * gridSizeX;
        const newY = Math.round((e.clientY - offsetY) / gridSizeY) * gridSizeY;

        activeDraggable.style.left = `${newX}px`;
        activeDraggable.style.top = `${newY}px`;
    }
}

// Handle the camera form submission via AJAX
document.getElementById('addCameraForm').addEventListener('submit', function(event) {
    event.preventDefault(); // Prevent the default form submission

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

// Call fetchAndRenderCameras on page load
window.onload = function() {
    fetchAndRenderCameras();

    // Auto-hiding toolbar functionality
    const toolbar = document.getElementById('toolbar');
    const hoverArea = document.querySelector('.toolbar-hover-area');

    hoverArea.addEventListener('mouseenter', () => {
        toolbar.classList.add('visible');
    });

    toolbar.addEventListener('mouseleave', () => {
        toolbar.classList.remove('visible');
    });
};
