// Temperature Monitoring Dashboard
// Comprehensive client-side functionality for sensor monitoring system

/**
 * Global Application State Management
 * 
 * Centralizes the state of the dashboard, tracking:
 * - Currently selected sensor
 * - Sensor history for charting
 * - Chart instance
 * - Refresh interval
 */
const AppState = {
    selectedSensor: null,     // Currently selected sensor address
    sensorHistory: {},        // Temperature history for all sensors
    chart: null,              // Chart.js instance for visualization
    refreshInterval: null     // Interval for automatic data refresh
};

/**
 * Logging Utility
 * 
 * Provides a centralized logging mechanism with different log levels
 * Helps in debugging and tracking application behavior
 */
const Logger = {
    // Standard logging with context
    log(message, data = null) {
        console.log(`[Dashboard] ${message}`, data || '');
    },
    
    // Error logging with detailed information
    error(message, error = null) {
        console.error(`[Dashboard Error] ${message}`, error || '');
    },
    
    // Warning logging for non-critical issues
    warn(message) {
        console.warn(`[Dashboard Warning] ${message}`);
    }
};

/**
 * Authentication and Network Utilities
 * 
 * Handles authentication-related operations:
 * - Checking authentication status
 * - Redirecting to login
 * - Handling logout
 * - Displaying error messages
 */
const AuthUtils = {
    /**
     * Check authentication status by attempting to fetch sensors
     * @returns {Promise<boolean>} Authentication status
     */
    async checkAuth() {
        try {
            Logger.log('Attempting authentication check...');
            
            const response = await fetch('/api/sensors', {
                credentials: 'include',  // Important for session-based auth
                method: 'GET'
            });
            
            // Detailed logging of authentication response
            Logger.log('Authentication Response', {
                status: response.status,
                ok: response.ok
            });

            // Handle different authentication scenarios
            switch (response.status) {
                case 200:  // Successful authentication
                    Logger.log('Authentication successful');
                    return true;
                case 401:  // Unauthorized
                    Logger.warn('Unauthorized access');
                    this.redirectToLogin();
                    return false;
                case 403:  // Forbidden
                    Logger.error('Access forbidden');
                    this.showError('Access denied. Contact administrator.');
                    return false;
                default:
                    Logger.error(`Unexpected authentication response: ${response.status}`);
                    return false;
            }
        } catch (error) {
            // Comprehensive error handling
            Logger.error('Authentication Check Failed', {
                message: error.message,
                name: error.name,
                stack: error.stack
            });

            // Different handling for network-related errors
            if (error instanceof TypeError) {
                this.showError('Network error. Check your connection.');
            }

            this.redirectToLogin();
            return false;
        }
    },

    /**
     * Redirect to login page
     */
    redirectToLogin() {
        Logger.warn('Redirecting to login page');
        window.location.href = '/login';
    },

    /**
     * Show error message to the user
     * @param {string} message - Error message to display
     */
    showError(message) {
        console.error(message);
        const alertDiv = document.createElement('div');
        alertDiv.className = 'bg-red-100 border border-red-400 text-red-700 px-4 py-3 rounded relative';
        alertDiv.textContent = message;
        
        const alertContainer = document.getElementById('alertContainer');
        if (alertContainer) {
            alertContainer.appendChild(alertDiv);
            
            // Auto-remove error after 5 seconds
            setTimeout(() => {
                if (alertDiv.parentNode) {
                    alertContainer.removeChild(alertDiv);
                }
            }, 5000);
        }
    },

    /**
     * Logout the current user
     */
    async logout() {
        try {
            const response = await fetch('/api/logout', {
                method: 'POST',
                credentials: 'include'
            });
            
            if (response.ok) {
                this.redirectToLogin();
            } else {
                throw new Error('Logout failed');
            }
        } catch (error) {
            this.showError('Logout failed. Please try again.');
        }
    }
};

/**
 * Sensor Selection and Persistence Management
 * 
 * Handles saving, retrieving, and managing sensor selections
 * Provides local storage integration for user preferences
 */
const SensorSelectionManager = {
    // Key for storing selected sensors in localStorage
    STORAGE_KEY: 'selectedChartSensors',

    /**
     * Save selected sensors to localStorage
     * @param {Array} sensors - List of selected sensor addresses
     */
    saveSelectedSensors(sensors) {
        try {
            localStorage.setItem(this.STORAGE_KEY, JSON.stringify(sensors));
            Logger.log('Saved selected sensors:', sensors);
        } catch (error) {
            Logger.error('Failed to save sensor selections', error);
        }
    },

    /**
     * Retrieve previously selected sensors
     * @returns {Array} List of selected sensor addresses
     */
    getSelectedSensors() {
        try {
            const savedSelections = localStorage.getItem(this.STORAGE_KEY);
            return savedSelections ? JSON.parse(savedSelections) : [];
        } catch (error) {
            Logger.error('Failed to retrieve saved sensor selections', error);
            return [];
        }
    },

    /**
     * Clear saved sensor selections
     */
    clearSelectedSensors() {
        try {
            localStorage.removeItem(this.STORAGE_KEY);
            Logger.log('Cleared saved sensor selections');
        } catch (error) {
            Logger.error('Failed to clear sensor selections', error);
        }
    }
};

/**
 * Chart Visualization Utilities
 * 
 * Manages chart creation, updating, and sensor selection
 */
const ChartUtils = {
    /**
     * Initialize the temperature history chart
     * @returns {Chart} Initialized Chart.js instance
     */
    initChart() {
        const chartCanvas = document.getElementById('temperatureChart');
        
        if (!chartCanvas) {
            Logger.error('Temperature chart canvas not found!');
            return null;
        }

        const ctx = chartCanvas.getContext('2d');
        
        return new Chart(ctx, {
            type: 'line',
            data: {
                datasets: []
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                scales: {
                    x: {
                        type: 'time',
                        time: {
                            unit: 'minute',
                            displayFormats: {
                                minute: 'HH:mm'
                            }
                        },
                        title: {
                            display: true,
                            text: 'Time'
                        }
                    },
                    y: {
                        title: {
                            display: true,
                            text: 'Temperature (°C)'
                        }
                    }
                },
                plugins: {
                    legend: {
                        display: true,
                        position: 'top'
                    }
                }
            }
        });
    },

    /**
     * Update chart with current sensor data
     */
    updateChart() {
        if (!AppState.chart) {
            Logger.error('Chart not initialized');
            return;
        }

        // Get saved selected sensors
        const selectedSensors = SensorSelectionManager.getSelectedSensors();
        
        // Clear existing datasets
        AppState.chart.data.datasets = [];

        // Create datasets for each sensor
        Object.entries(AppState.sensorHistory).forEach(([address, history]) => {
            // Skip sensors not in saved selections (if any selections exist)
            if (selectedSensors.length > 0 && !selectedSensors.includes(address)) {
                return;
            }

            if (history.length === 0) return;

            const sensorName = this.getSensorName(address);

            AppState.chart.data.datasets.push({
                label: sensorName,
                data: history.map(entry => ({
                    x: entry.time,
                    y: entry.temp
                })),
                borderColor: this.getRandomColor(),
                tension: 0.1
            });
        });

        // Update chart
        AppState.chart.update();
    },

    /**
     * Generate a random color for chart lines
     * @returns {string} Randomly generated hex color
     */
    getRandomColor() {
        const letters = '0123456789ABCDEF';
        let color = '#';
        for (let i = 0; i < 6; i++) {
            color += letters[Math.floor(Math.random() * 16)];
        }
        return color;
    },

    /**
     * Get sensor name from select dropdown
     * @param {string} address - Sensor address
     * @returns {string} Sensor name or default identifier
     */
    getSensorName(address) {
        const sensorSelect = document.getElementById('sensorSelect');
        if (!sensorSelect) {
            Logger.error('Sensor select dropdown not found');
            return `Sensor ${address.slice(-4)}`;
        }
        
        const option = sensorSelect.querySelector(`option[value="${address}"]`);
        return option ? option.textContent : `Sensor ${address.slice(-4)}`;
    }
};

/**
 * Sensor Data Management
 * 
 * Handles fetching, processing, and displaying sensor data
 */
const SensorManager = {
    /**
     * Fetch sensor data from the server
     */
    async fetchSensors() {
        try {
            Logger.log('Fetching sensors...');
            
            const response = await fetch('/api/sensors', {
                credentials: 'include'
            });

            // Detailed response logging
            Logger.log('Sensor Fetch Response', {
                status: response.status,
                statusText: response.statusText
            });

            if (!response.ok) {
                throw new Error(`Sensor fetch failed: ${response.status}`);
            }

            const sensors = await response.json();
            Logger.log('Received Sensors', sensors);

            // Validate sensor data
            if (!Array.isArray(sensors)) {
                throw new Error('Invalid sensor data format');
            }

            this.processSensors(sensors);
        } catch (error) {
            Logger.error('Sensor Fetch Error', {
                message: error.message,
                name: error.name,
                stack: error.stack
            });
            
            AuthUtils.showError('Unable to retrieve sensor data');
        }
    },

    /**
     * Process and display sensor data
     * @param {Array} sensors - List of sensor data
     */
    processSensors(sensors) {
        try {
            // Update various dashboard components
            this.updateSensorSelect(sensors);
            this.updateSensorList(sensors);
            this.updateCurrentSensor(sensors);
            this.updateSensorHistory(sensors);

            // Initialize or update chart
            if (!AppState.chart) {
                AppState.chart = ChartUtils.initChart();
            }
            
            ChartUtils.updateChart();
        } catch (error) {
            Logger.error('Sensor Processing Error', error);
        }
    },

    /**
     * Update sensor selection dropdown
     * @param {Array} sensors - List of sensors
     */
    updateSensorSelect(sensors) {
        const sensorSelect = document.getElementById('sensorSelect');
        if (!sensorSelect) return;

        // Clear existing options except the first
        while (sensorSelect.options.length > 1) {
            sensorSelect.remove(1);
        }

        // Add new sensor options
        sensors.forEach(sensor => {
            const option = document.createElement('option');
            option.value = sensor.address;
            option.textContent = sensor.name || `Sensor ${sensor.address.slice(-4)}`;
            
            if (sensor.valid) {
                option.textContent += ` (${sensor.temperature.toFixed(1)}°C)`;
            }

            sensorSelect.appendChild(option);
        });
    },

    /**
     * Update sensor list display with selection capability
     * @param {Array} sensors - List of sensors
     */
    updateSensorList(sensors) {
        const sensorList = document.getElementById('sensorList');
        if (!sensorList) return;

        // Get current selected sensors
        const selectedSensors = SensorSelectionManager.getSelectedSensors();

        sensorList.innerHTML = '';

        sensors.forEach(sensor => {
            const sensorCard = document.createElement('div');
            
            // Determine if this sensor is currently selected
            const isSelected = selectedSensors.length === 0 || 
                               selectedSensors.includes(sensor.address);

            sensorCard.className = `p-4 rounded-lg shadow cursor-pointer transition-all 
                ${isSelected ? 'bg-white' : 'bg-gray-100 opacity-50'}
                ${sensor.valid ? '' : 'bg-red-50'}
                hover:shadow-md`;
            
            sensorCard.innerHTML = `
                <div class="flex justify-between items-center">
                    <div class="font-medium text-gray-800">
                        ${sensor.name || `Sensor ${sensor.address.slice(-4)}`}
                    </div>
                    <div class="text-lg font-bold ${sensor.valid ? 'text-blue-600' : 'text-red-600'}">
                        ${sensor.valid ? sensor.temperature.toFixed(1) + '°C' : 'Error'}
                    </div>
                </div>
                ${!sensor.valid ? '<div class="text-xs text-red-500 mt-1">Invalid reading</div>' : ''}
            `;

            // Add click event to toggle sensor selection
            sensorCard.addEventListener('click', () => {
                const currentSelections = SensorSelectionManager.getSelectedSensors();
                
                let newSelections;
                if (currentSelections.includes(sensor.address)) {
                    // Remove sensor if already selected
                    newSelections = currentSelections.filter(addr => addr !== sensor.address);
                } else {
                    // Add sensor to selections
                    newSelections = [...currentSelections, sensor.address];
                }

                // Save new selections
                SensorSelectionManager.saveSelectedSensors(newSelections);

                // Refresh sensor list and chart
                this.updateSensorList(sensors);
                ChartUtils.updateChart();
            });

            sensorList.appendChild(sensorCard);
        });
    },

    /**
     * Update current sensor display
     * @param {Array} sensors - List of sensors
     */
    updateCurrentSensor(sensors) {
        const currentTemperature = document.getElementById('currentTemperature');
        const currentSensorName = document.getElementById('currentSensorName');

        if (!currentTemperature || !currentSensorName) return;

        // If no sensor selected, show default
        if (!AppState.selectedSensor) {
            currentTemperature.textContent = '--.-°C';
            currentSensorName.textContent = 'No sensor selected';
            return;
        }

        const currentSensor = sensors.find(s => s.address === AppState.selectedSensor);
        
        if (currentSensor && currentSensor.valid) {
            currentTemperature.textContent = `${currentSensor.temperature.toFixed(1)}°C`;
            currentSensorName.textContent = currentSensor.name || `Sensor ${currentSensor.address.slice(-4)}`;
        }
    },

    /**
     * Update sensor history for charting
     * @param {Array} sensors - List of sensors
     */
    updateSensorHistory(sensors) {
        sensors.forEach(sensor => {
            // Only track valid sensors with meaningful temperature
            if (sensor.valid && sensor.temperature !== -127) {
                if (!AppState.sensorHistory[sensor.address]) {
                    AppState.sensorHistory[sensor.address] = [];
                }

                AppState.sensorHistory[sensor.address].push({
                    time: new Date(),
                    temp: sensor.temperature
                });

                // Limit history to last 20 entries
                if (AppState.sensorHistory[sensor.address].length > 20) {
                    AppState.sensorHistory[sensor.address].shift();
                }
            }
        });
    }
};

/**
 * Dashboard Initialization
 * 
 * Sets up the entire dashboard, including authentication, 
 * event listeners, and periodic data refresh
 */
async function initializeDashboard() {
    Logger.log('Initializing Dashboard...');

    try {
        // Authentication check
        const isAuthenticated = await AuthUtils.checkAuth();
        if (!isAuthenticated) {
            Logger.error('Authentication failed');
            return;
        }

        // Bind sensor fetch method to ensure correct context
        const fetchSensors = SensorManager.fetchSensors.bind(SensorManager);

        // Initial sensor data fetch
        await fetchSensors();

        // Set up event listeners
        const setupEventListeners = () => {
            const ui = {
                sensorSelect: document.getElementById('sensorSelect'),
                refreshButton: document.getElementById('refreshButton'),
                logoutButton: document.getElementById('logoutButton')
            };

            if (ui.sensorSelect) {
                ui.sensorSelect.addEventListener('change', (e) => {
                    Logger.log('Sensor selection changed:', e.target.value);
                    AppState.selectedSensor = e.target.value;
                    fetchSensors();
                });
            } else {
                Logger.warn('Sensor select dropdown not found');
            }

            if (ui.refreshButton) {
                ui.refreshButton.addEventListener('click', () => {
                    Logger.log('Manual refresh triggered');
                    fetchSensors();
                });
            } else {
                Logger.warn('Refresh button not found');
            }

            if (ui.logoutButton) {
                ui.logoutButton.addEventListener('click', () => {
                    Logger.log('Logout initiated');
                    AuthUtils.logout();
                });
            } else {
                Logger.warn('Logout button not found');
            }
        };

        setupEventListeners();

        // Set up periodic refresh
        if (AppState.refreshInterval) {
            clearInterval(AppState.refreshInterval);
        }
        AppState.refreshInterval = setInterval(fetchSensors, 5000);
        Logger.log('Periodic refresh interval set');

        // Initialize Lucide icons if available
        if (typeof lucide !== 'undefined' && lucide.createIcons) {
            lucide.createIcons();
            Logger.log('Lucide icons initialized');
        } else {
            Logger.warn('Lucide icons not available');
        }

    } catch (error) {
        Logger.error('Dashboard Initialization Failed', error);
        AuthUtils.showError('Failed to initialize dashboard');
    }
}

/**
 * Global Error Handling
 * 
 * Catches and logs any unhandled promise rejections
 * Provides a safety net for unexpected errors
 */
window.addEventListener('unhandledrejection', (event) => {
    Logger.error('Unhandled Promise Rejection', {
        reason: event.reason,
        promise: event.promise
    });
    AuthUtils.showError('An unexpected error occurred');
});

/**
 * Dashboard Initialization Trigger
 * 
 * Starts the dashboard when the DOM is fully loaded
 * Ensures all elements are available before initialization
 */
document.addEventListener('DOMContentLoaded', () => {
    Logger.log('DOM fully loaded, starting dashboard initialization');
    initializeDashboard();
});