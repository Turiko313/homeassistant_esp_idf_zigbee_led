// Converter Zigbee2MQTT pour ESP32-H2 WS2812 avec support des effets
// Fichier a placer dans : zigbee2mqtt/data/converters/WS2812_Light.mjs

const fz = require('zigbee-herdsman-converters/converters/fromZigbee');
const tz = require('zigbee-herdsman-converters/converters/toZigbee');
const exposes = require('zigbee-herdsman-converters/lib/exposes');
const reporting = require('zigbee-herdsman-converters/lib/reporting');
const e = exposes.presets;
const ea = exposes.access;

const definition = {
    zigbeeModel: ['WS2812_Light'],
    model: 'WS2812_ESP32H2',
    vendor: 'Custom',
    description: 'ESP32-H2 WS2812 LED Strip Controller avec effets',
    
    exposes: [
        e.light_brightness_colorxy(),
        exposes.enum('effect', ea.SET, ['none', 'rainbow', 'strobe', 'twinkle'])
            .withDescription('Effet d\'animation'),
        exposes.numeric('speed_rainbow', ea.SET)
            .withValueMin(1)
            .withValueMax(255)
            .withDescription('Vitesse Rainbow (1=lent, 255=rapide)'),
        exposes.numeric('speed_strobe', ea.SET)
            .withValueMin(1)
            .withValueMax(255)
            .withDescription('Vitesse Strobe (1=lent, 255=rapide)'),
        exposes.numeric('speed_twinkle', ea.SET)
            .withValueMin(1)
            .withValueMax(255)
            .withDescription('Vitesse Twinkle (1=lent, 255=rapide)'),
    ],
    
    fromZigbee: [
        fz.on_off,
        fz.brightness,
        fz.color_colortemp,
    ],
    
    toZigbee: [
        tz.light_onoff_brightness,
        tz.light_color,
        {
            key: ['effect'],
            convertSet: async (entity, key, value, meta) => {
                const effects = {
                    'none': 0,
                    'rainbow': 1,
                    'strobe': 2,
                    'twinkle': 3,
                };
                
                const effectId = effects[value.toLowerCase()] || 0;
                
                await entity.write('lightingColorCtrl', {
                    '61440': {value: effectId, type: 0x20},
                }, {
                    manufacturerCode: 0x1234,
                });
                
                return {state: {effect: value}};
            },
        },
        {
            key: ['speed_rainbow'],
            convertSet: async (entity, key, value, meta) => {
                const speed = Math.max(1, Math.min(255, value));
                await entity.write('lightingColorCtrl', {
                    '61441': {value: speed, type: 0x20},
                }, {manufacturerCode: 0x1234});
                return {state: {speed_rainbow: speed}};
            },
        },
        {
            key: ['speed_strobe'],
            convertSet: async (entity, key, value, meta) => {
                const speed = Math.max(1, Math.min(255, value));
                await entity.write('lightingColorCtrl', {
                    '61442': {value: speed, type: 0x20},
                }, {manufacturerCode: 0x1234});
                return {state: {speed_strobe: speed}};
            },
        },
        {
            key: ['speed_twinkle'],
            convertSet: async (entity, key, value, meta) => {
                const speed = Math.max(1, Math.min(255, value));
                await entity.write('lightingColorCtrl', {
                    '61443': {value: speed, type: 0x20},
                }, {manufacturerCode: 0x1234});
                return {state: {speed_twinkle: speed}};
            },
        },
    ],
    
    configure: async (device, coordinatorEndpoint, logger) => {
        const endpoint = device.getEndpoint(10);
        
        await reporting.bind(endpoint, coordinatorEndpoint, [
            'genOnOff',
            'genLevelCtrl',
            'lightingColorCtrl',
        ]);
        
        await reporting.onOff(endpoint);
        await reporting.brightness(endpoint);
    },
};

module.exports = definition;
