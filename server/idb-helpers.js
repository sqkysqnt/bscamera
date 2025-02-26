// idb-helpers.js

const DB_NAME = 'channelPreferencesDB';
const STORE_NAME = 'channelSubs';
const DB_VERSION = 1;

// Open the IndexedDB
function openChannelDB() {
    return new Promise((resolve, reject) => {
        const request = indexedDB.open(DB_NAME, DB_VERSION);

        request.onupgradeneeded = (event) => {
            const db = event.target.result;
            if (!db.objectStoreNames.contains(STORE_NAME)) {
                db.createObjectStore(STORE_NAME, { keyPath: 'id' });
            }
        };

        request.onsuccess = (event) => resolve(event.target.result);
        request.onerror = (event) => reject(event.target.error);
    });
}

// Save subscribed channels to IndexedDB
async function saveChannelSubscriptionsToIDB(channelsArray) {
    try {
        const db = await openChannelDB();
        const tx = db.transaction(STORE_NAME, 'readwrite');
        const store = tx.objectStore(STORE_NAME);
        
        // Store a single record with key 'subscribed'
        store.put({ id: 'subscribed', channels: channelsArray });

        await tx.complete;
        db.close();
    } catch (error) {
        console.error('Error saving to IndexedDB:', error);
    }
}

// Get subscribed channels from IndexedDB
async function getChannelSubscriptionsFromIDB() {
    return new Promise(async (resolve, reject) => {
        try {
            const db = await openChannelDB();
            const tx = db.transaction(STORE_NAME, 'readonly');
            const store = tx.objectStore(STORE_NAME);
            const request = store.get('subscribed');

            request.onsuccess = () => {
                const result = request.result;
                db.close();
                resolve(result ? result.channels : []);
            };

            request.onerror = (event) => {
                db.close();
                reject(event.target.error);
            };
        } catch (error) {
            reject(error);
        }
    });
}

// Export functions for use in service-worker.js
export { saveChannelSubscriptionsToIDB, getChannelSubscriptionsFromIDB };
