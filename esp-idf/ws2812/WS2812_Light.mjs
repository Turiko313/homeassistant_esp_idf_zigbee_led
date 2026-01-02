/**
 * Converter externe Zigbee2MQTT pour WS2812_Light
 * 
 * Copier ce contenu dans l'interface Zigbee2MQTT :
 * Settings ? External converters ? Add new converter
 * 
 * Puis redémarrez Zigbee2MQTT et re-pairez l'appareil.
 */

import {light} from 'zigbee-herdsman-converters/lib/modernExtend';

const definition = {
    zigbeeModel: ['WS2812_Light'],
    model: 'WS2812_Light',
    vendor: 'ESP32-Zigbee',
    description: 'WS2812 LED Strip Controller',
    
    extend: [
        light({
            colorTemp: false,           // ? Désactive Color Temperature
            color: {
                modes: ['xy'],          // Seulement XY (pas HS)
                enhancedHue: false,
            },
            powerOnBehavior: false,     // ? Désactive Power-on behavior
            effect: false,              // ? Désactive Effects
        }),
    ],
    
    endpoint: (device) => {
        return {default: 10};
    },
};

export default definition;
