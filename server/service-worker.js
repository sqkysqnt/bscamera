// service-worker.js
self.addEventListener('push', function(event) {
  console.log('Entered service worker');
  const data = event.data.json();
  console.log('Push received:', data);

  // Prepare notification options (includes custom icon)
  const options = {
    body: data.message,
    icon: '/static/images/icon.png',   // <-- your custom icon
    badge: '/static/images/badge.png',   // or remove if not needed
  };

  event.waitUntil(
    // 1. Check all open clients (browser tabs/windows)
    self.clients.matchAll({
      type: 'window',
      includeUncontrolled: true
    }).then(function(clientList) {
      // 2. If there's at least one client currently visible, skip the notification
      //    (You can refine this logic, e.g. only skip if *this* exact page is visible)
      const isWindowFocused = clientList.some(client => {
        // Some browsers support `client.visibilityState === 'visible'`
        // Others only let us see `client.focused`.
        return client.visibilityState === 'visible' || client.focused;
      });

      if (isWindowFocused) {
        // If any tab is visible/focused, do NOT show the notification
        console.log('No notification because page is in the foreground');
        return;
      }

      // Otherwise, show the push
      return self.registration.showNotification(data.title, options);
    })
  );
});

self.addEventListener('notificationclick', function(event) {
  event.notification.close();
  event.waitUntil(
    // #2. This opens /messages in a new tab or focuses it if already open
    clients.openWindow('/messages')
  );
});
