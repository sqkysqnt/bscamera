// service-worker.js
self.importScripts('/idb-helpers.js'); 
// (for example, a file that defines openChannelDB() or getChannelSubs() 
// so the SW can also read from the same DB.)

self.addEventListener('push', function(event) {
  console.log('Entered service worker');
  const data = event.data.json();
  console.log('Push received:', data);

  event.waitUntil((async () => {
    // 1. Get the user’s subscribed channels from IndexedDB
    const subscribedChannels = await getChannelSubscriptionsFromIDB();

    // 2. If the user isn’t subscribed to this channel, do nothing
    if (!subscribedChannels.includes(data.channel)) {
      console.log(`Skipping push for channel '${data.channel}' because user did not subscribe.`);
      return;
    }

    // 3. If user has a visible tab, skip the notification (your existing logic).
    const clientList = await self.clients.matchAll({
      type: 'window',
      includeUncontrolled: true
    });
    const isWindowFocused = clientList.some(client => {
      // Some browsers have `client.visibilityState === 'visible'`
      // Others might only let you see `client.focused`.
      return client.visibilityState === 'visible' || client.focused;
    });

    if (isWindowFocused) {
      console.log('No notification because page is visible in foreground');
      return;
    }

    // 4. Otherwise, show the push
    const options = {
      body: data.message,
      icon: '/static/images/icon.png',
      badge: '/static/images/badge.png',
    };
    return self.registration.showNotification(data.title, options);
  })());
});

self.addEventListener('notificationclick', function(event) {
  event.notification.close();
  event.waitUntil(
    // This opens /messages in a new tab or focuses it if already open
    clients.openWindow('/messages')
  );
});
