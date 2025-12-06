/**
 * HomeSense WiFi Setup - Client-Side Logic
 * Handles WiFi scanning, form validation, and connection feedback
 */

// DOM Elements
const wifiForm = document.getElementById('wifiForm');
const ssidInput = document.getElementById('ssid');
const passwordInput = document.getElementById('password');
const submitBtn = document.getElementById('submitBtn');
const scanBtn = document.getElementById('scanBtn');
const scanLoader = document.getElementById('scanLoader');
const networkList = document.getElementById('networkList');
const statusMsg = document.getElementById('status');
const togglePasswordBtn = document.getElementById('togglePassword');

// State
let selectedNetwork = null;

/**
 * Initialize event listeners
 */
function init() {
    // Form submission
    wifiForm.addEventListener('submit', handleFormSubmit);

    // WiFi scan button
    scanBtn.addEventListener('click', scanNetworks);

    // Password toggle
    togglePasswordBtn.addEventListener('click', togglePasswordVisibility);

    // Auto-scan on page load
    setTimeout(scanNetworks, 500);
}

/**
 * Scan for available WiFi networks
 */
async function scanNetworks() {
    try {
        scanBtn.disabled = true;
        scanLoader.classList.remove('hidden');
        networkList.classList.add('hidden');
        networkList.innerHTML = '';

        showStatus('Scanning for networks...', 'info');

        const response = await fetch('/api/scan');
        
        if (!response.ok) {
            throw new Error('Scan failed');
        }

        const data = await response.json();
        const networks = data.networks || [];

        if (networks.length === 0) {
            showStatus('No networks found. Please try again.', 'error');
            return;
        }

        displayNetworks(networks);
        showStatus(`Found ${networks.length} network${networks.length > 1 ? 's' : ''}`, 'success');

    } catch (error) {
        console.error('Scan error:', error);
        showStatus('Failed to scan networks. Please try again.', 'error');
    } finally {
        scanBtn.disabled = false;
        scanLoader.classList.add('hidden');
    }
}

/**
 * Display scanned networks
 */
function displayNetworks(networks) {
    networkList.innerHTML = '';
    networkList.classList.remove('hidden');

    // Remove duplicates and sort by signal strength
    const uniqueNetworks = Array.from(
        new Map(networks.map(n => [n.ssid, n])).values()
    ).sort((a, b) => b.rssi - a.rssi);

    uniqueNetworks.forEach(network => {
        const item = createNetworkItem(network);
        networkList.appendChild(item);
    });
}

/**
 * Create network list item element
 */
function createNetworkItem(network) {
    const item = document.createElement('div');
    item.className = 'network-item';
    item.onclick = () => selectNetwork(network);

    // Network name
    const nameDiv = document.createElement('div');
    nameDiv.className = 'network-name';
    nameDiv.textContent = network.ssid;

    // Signal strength indicator
    const signalDiv = document.createElement('div');
    signalDiv.className = 'network-signal';
    
    const signalStrength = getSignalStrength(network.rssi);
    for (let i = 1; i <= 4; i++) {
        const bar = document.createElement('div');
        bar.className = 'signal-bar';
        if (i <= signalStrength) {
            bar.style.opacity = '1';
        } else {
            bar.style.opacity = '0.3';
        }
        signalDiv.appendChild(bar);
    }

    item.appendChild(nameDiv);
    item.appendChild(signalDiv);

    return item;
}

/**
 * Calculate signal strength (1-4 bars)
 */
function getSignalStrength(rssi) {
    if (rssi >= -50) return 4;
    if (rssi >= -60) return 3;
    if (rssi >= -70) return 2;
    return 1;
}

/**
 * Select a network from the list
 */
function selectNetwork(network) {
    // Remove previous selection
    const items = networkList.querySelectorAll('.network-item');
    items.forEach(item => item.classList.remove('selected'));

    // Add selection to clicked item
    event.target.closest('.network-item').classList.add('selected');

    // Update form
    selectedNetwork = network;
    ssidInput.value = network.ssid;
    
    // Focus password input
    passwordInput.focus();
}

/**
 * Toggle password visibility
 */
function togglePasswordVisibility() {
    const type = passwordInput.type === 'password' ? 'text' : 'password';
    passwordInput.type = type;

    // Toggle icons
    const eyeIcon = togglePasswordBtn.querySelector('.icon-eye');
    const eyeOffIcon = togglePasswordBtn.querySelector('.icon-eye-off');
    
    eyeIcon.classList.toggle('hidden');
    eyeOffIcon.classList.toggle('hidden');
}

/**
 * Handle form submission
 */
async function handleFormSubmit(e) {
    e.preventDefault();

    const ssid = ssidInput.value.trim();
    const password = passwordInput.value;

    // Validation
    if (!ssid) {
        showStatus('Please enter a network name', 'error');
        ssidInput.focus();
        return;
    }

    if (ssid.length > 32) {
        showStatus('Network name is too long (max 32 characters)', 'error');
        ssidInput.focus();
        return;
    }

    if (password.length > 64) {
        showStatus('Password is too long (max 64 characters)', 'error');
        passwordInput.focus();
        return;
    }

    // Show loading state
    setLoadingState(true);
    showStatus('Connecting to network...', 'info');

    try {
        // URL encode parameters
        const params = new URLSearchParams({
            ssid: ssid,
            password: password
        });

        const response = await fetch(`/api/connect?${params.toString()}`, {
            method: 'GET',
        });

        if (!response.ok) {
            throw new Error('Connection failed');
        }

        // Success
        showStatus('✓ Connected! Device is rebooting...', 'success');
        
        // Disable form
        wifiForm.querySelectorAll('input, button').forEach(el => {
            el.disabled = true;
        });

        // Show redirect message
        setTimeout(() => {
            showStatus('Device will restart in a few seconds. You can close this page.', 'success');
        }, 2000);

    } catch (error) {
        console.error('Connection error:', error);
        showStatus('Failed to connect. Please check credentials and try again.', 'error');
        setLoadingState(false);
    }
}

/**
 * Set loading state for submit button
 */
function setLoadingState(isLoading) {
    if (isLoading) {
        submitBtn.classList.add('loading');
        submitBtn.disabled = true;
        submitBtn.querySelector('.btn-text').style.display = 'none';
        submitBtn.querySelector('.btn-icon').style.display = 'none';
        submitBtn.querySelector('.btn-loader').classList.remove('hidden');
    } else {
        submitBtn.classList.remove('loading');
        submitBtn.disabled = false;
        submitBtn.querySelector('.btn-text').style.display = 'inline';
        submitBtn.querySelector('.btn-icon').style.display = 'inline';
        submitBtn.querySelector('.btn-loader').classList.add('hidden');
    }
}

/**
 * Show status message
 */
function showStatus(message, type = 'info') {
    statusMsg.textContent = message;
    statusMsg.className = `status ${type}`;
    statusMsg.classList.remove('hidden');

    // Auto-hide success messages
    if (type === 'success') {
        setTimeout(() => {
            // Don't hide if it contains "rebooting" or "restart"
            if (!message.includes('reboot') && !message.includes('restart')) {
                statusMsg.classList.add('hidden');
            }
        }, 5000);
    }
}

/**
 * URL decode helper (handles special characters)
 */
function urlDecode(str) {
    return decodeURIComponent((str + '').replace(/\+/g, '%20'));
}

// Initialize when DOM is ready
if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', init);
} else {
    init();
}
