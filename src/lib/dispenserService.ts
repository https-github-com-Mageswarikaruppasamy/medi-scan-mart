/**
 * Dispenser Service
 * 
 * Handles communication with the ESP32 medicine dispenser controller.
 * Sends dispense commands after successful payment.
 */

import { MedicineWithPrice } from "./medicineData";

// =============================================================================
// CONFIGURATION
// =============================================================================

/**
 * ESP32 IP Address - Change this to match your ESP32's IP
 * You can find this in the Arduino Serial Monitor after upload
 */
const ESP32_IP = "10.226.144.6";

/**
 * ESP32 Server Port (default: 80)
 */
const ESP32_PORT = 80;

/**
 * Request timeout in milliseconds
 */
const REQUEST_TIMEOUT_MS = 10000;

// =============================================================================
// TYPES
// =============================================================================

export interface DispenseRequest {
    paymentId: string;
    paymentStatus: "success" | "failed";
    items: Array<{
        name: string;
        qty: number;
    }>;
}

export interface DispenseResponse {
    success: boolean;
    dispensedCount?: number;
    message?: string;
    error?: string;
}

// =============================================================================
// MAIN FUNCTION
// =============================================================================

/**
 * Send a dispense command to the ESP32 controller
 * 
 * @param paymentId - The Razorpay payment ID
 * @param items - Array of medicines with quantities to dispense
 * @returns Promise with dispense result
 */
export async function sendDispenseCommand(
    paymentId: string,
    items: MedicineWithPrice[]
): Promise<DispenseResponse> {
    const url = `http://${ESP32_IP}:${ESP32_PORT}/dispense`;

    const payload: DispenseRequest = {
        paymentId,
        paymentStatus: "success",
        items: items.map((item) => ({
            name: item.name,
            qty: item.qty,
        })),
    };

    console.log("[DispenserService] Sending dispense command to ESP32:", url);
    console.log("[DispenserService] Payload:", JSON.stringify(payload, null, 2));

    try {
        // Create AbortController for timeout
        const controller = new AbortController();
        const timeoutId = setTimeout(() => controller.abort(), REQUEST_TIMEOUT_MS);

        const response = await fetch(url, {
            method: "POST",
            headers: {
                "Content-Type": "application/json",
            },
            body: JSON.stringify(payload),
            signal: controller.signal,
        });

        clearTimeout(timeoutId);

        const data: DispenseResponse = await response.json();

        console.log("[DispenserService] ESP32 response:", data);

        if (!response.ok) {
            console.error("[DispenserService] ESP32 returned error:", response.status);
            return {
                success: false,
                error: data.error || `HTTP ${response.status}`,
            };
        }

        return data;
    } catch (error) {
        // Handle network errors
        if (error instanceof Error) {
            if (error.name === "AbortError") {
                console.error("[DispenserService] Request timed out after", REQUEST_TIMEOUT_MS, "ms");
                return {
                    success: false,
                    error: "Connection timed out. Is the ESP32 powered on?",
                };
            }

            console.error("[DispenserService] Network error:", error.message);
            return {
                success: false,
                error: `Network error: ${error.message}`,
            };
        }

        console.error("[DispenserService] Unknown error:", error);
        return {
            success: false,
            error: "Unknown error occurred",
        };
    }
}

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

/**
 * Check if the ESP32 dispenser is online
 * 
 * @returns Promise with online status
 */
export async function checkDispenserStatus(): Promise<{
    online: boolean;
    ipAddress?: string;
    error?: string;
}> {
    const url = `http://${ESP32_IP}:${ESP32_PORT}/status`;

    try {
        const controller = new AbortController();
        const timeoutId = setTimeout(() => controller.abort(), 3000);

        const response = await fetch(url, { signal: controller.signal });
        clearTimeout(timeoutId);

        if (response.ok) {
            const data = await response.json();
            return {
                online: true,
                ipAddress: data.ipAddress,
            };
        }

        return { online: false, error: `HTTP ${response.status}` };
    } catch {
        return { online: false, error: "Cannot reach ESP32" };
    }
}

/**
 * Get the configured ESP32 IP address
 */
export function getESP32IP(): string {
    return ESP32_IP;
}
