// Converter Zigbee2MQTT pour ESP32-H2 WS2812 avec support des effets
// Fichier à placer dans : zigbee2mqtt/data/converters/WS2812_Light.mjs
// Ou via l'interface Z2M : Settings ? External converters ? Add new converter

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
    
    // Exposition des fonctionnalités
    exposes: [
        e.light_brightness_colorxy(),
        exposes.enum('effect', ea.ALL, ['none', 'rainbow', 'strobe', 'twinkle'])
            .withDescription('Effet d\'animation'),
    ],
    
    // Convertisseurs FROM Zigbee (lecture)
    fromZigbee: [
        fz.on_off,
        fz.brightness,
        fz.color_colortemp,
        {
            cluster: 'lightingColorCtrl',
            type: ['attributeReport', 'readResponse'],
            convert: (model, msg, publish, options, meta) => {
                const result = {};
                // Attribut custom pour effet (ID 0xF000, manufacturer code 0x1234)
                if (msg.data.hasOwnProperty('61440')) { // 0xF000 = 61440
                    const effectId = msg.data['61440'];
                    const effects = ['none', 'rainbow', 'strobe', 'twinkle'];
                    result.effect = effects[effectId] || 'none';
                }
                return result;
            },
        },
    ],
    
    // Convertisseurs TO Zigbee (écriture)
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
                
                // Écrire dans l'attribut manufacturer-specific 0xF000
                // avec manufacturer code 0x1234
                // dataType 0x20 = ZCL_DATATYPE_UINT8 (DataType.uint8)
                await entity.write('lightingColorCtrl', {
                    '61440': {value: effectId, type: 0x20},  // 0xF000 = 61440, type 0x20 = uint8
                }, {
                    manufacturerCode: 0x1234,
                });
                
                return {state: {effect: value}};
            },
            convertGet: async (entity, key, meta) => {
                await entity.read('lightingColorCtrl', ['61440'], {
                    manufacturerCode: 0x1234,
                });
            },
        },
    ],
    
    // Configuration du reporting (optionnel)
    configure: async (device, coordinatorEndpoint, logger) => {
        const endpoint = device.getEndpoint(10);
        
        await reporting.bind(endpoint, coordinatorEndpoint, [
            'genOnOff',
            'genLevelCtrl',
            'lightingColorCtrl',
        ]);
        
        await reporting.onOff(endpoint);
        await reporting.brightness(endpoint);
        await reporting.colorXY(endpoint);
    },
};

module.exports = definition;
