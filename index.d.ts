// Type definitions for usb-detection 3.1.0
// Project: https://github.com/MadLittleMods/node-usb-detection
// Definitions by: Rob Moran <https://github.com/thegecko>

export interface Device {
    locationId: number;
    vendorId: number;
    productId: number;
    deviceName: string;
    manufacturer: string;
    serialNumber: string;
    deviceAddress: number;
}

export function find(vid: number, pid: number, callback: (error: any, devices: Device[]) => any): void;
export function find(vid: number, callback: (error: any, devices: Device[]) => any): void;
export function find(callback: (error: any, devices: Device[]) => any): void;
export function find(): Promise<Device[]>;

export function startMonitoring(): void;
export function stopMonitoring(): void;
export function on(event: string, callback: (device: Device) => void): void;

export const version: number;
