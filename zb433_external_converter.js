const m = require('zigbee-herdsman-converters/lib/modernExtend');
const exposes = require('zigbee-herdsman-converters/lib/exposes');
const e = exposes.presets;
const reporting = require('zigbee-herdsman-converters/lib/reporting');

module.exports = [{
  // Accepte plusieurs variantes possibles
  fingerprint: [
    {modelID: 'ZB433-Router', manufacturerName: 'Cesar RICHARD EI'},
  ],
  zigbeeModel: ['ZB433-Router'],
  model: 'ZB433-Router',
  vendor: 'Cesar RICHARD EI',
  description: 'CAME 433 TX Router',
  extend: [
    m.deviceEndpoints({endpoints: {portail_principal: 1, portail_parking: 2}}),
    m.onOff({powerOnBehavior: false, endpointNames: ['portail_principal', 'portail_parking']}),
  ],
  exposes: [
    e.switch().withEndpoint('portail_principal').withDescription('Commande du portail Principal'),
    e.switch().withEndpoint('portail_parking').withDescription('Commande du portail Parking'),
  ],
  meta: {multiEndpoint: true},
  configure: async (device, coordinatorEndpoint, logger) => {
    const ep1 = device.getEndpoint(1), ep2 = device.getEndpoint(2);
    await reporting.bind(ep1, coordinatorEndpoint, ['genOnOff']);
    await reporting.bind(ep2, coordinatorEndpoint, ['genOnOff']);
    await reporting.onOff(ep1); await reporting.onOff(ep2);
  },
}];
