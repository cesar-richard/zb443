const m = require('zigbee-herdsman-converters/lib/modernExtend');
const exposes = require('zigbee-herdsman-converters/lib/exposes');
const e = exposes.presets;
const reporting = require('zigbee-herdsman-converters/lib/reporting');
const {fromZigbee, toZigbee} = require('zigbee-herdsman-converters');

module.exports = [{
  fingerprint: [
    {modelID: 'ZB433-Router', manufacturerName: 'Cesar RICHARD EI'},
  ],
  zigbeeModel: ['ZB433-Router'],
  model: 'ZB433-Router',
  vendor: 'Cesar RICHARD EI',
  description: 'CAME 433 TX Router',
  extend: [
    m.deviceEndpoints({endpoints: {portail_principal: 1, portail_parking: 2}}),
    // Ne pas utiliser m.onOff() pour éviter les switches automatiques
  ],
  exposes: [
    // Exposer comme multistate choice (liste déroulante) avec une seule option = bouton poussoir
    // Utiliser des noms différents pour chaque endpoint pour qu'ils apparaissent séparément
    e.enum('portail_principal', exposes.access.SET, ['press'])
      .withEndpoint('portail_principal').withDescription('Commande du portail Principal'),
    e.enum('portail_parking', exposes.access.SET, ['press'])
      .withEndpoint('portail_parking').withDescription('Commande du portail Parking'),
  ],
  meta: {
    multiEndpoint: true,
  },
  toZigbee: [
    {
      key: ['portail_principal', 'portail_parking', 'state'],  // Gérer les deux endpoints et 'state' pour masquer les switches
      convertSet: async (entity, key, value, meta) => {
        // Déterminer l'endpoint selon la clé
        let endpointId;
        if (key === 'portail_principal') {
          endpointId = 1;
        } else if (key === 'portail_parking') {
          endpointId = 2;
        } else if (key === 'state') {
          // Si c'est 'state', utiliser meta.endpoint_name ou meta.endpoint_id
          if (meta.endpoint_name) {
            endpointId = meta.endpoint_name === 'portail_principal' ? 1 : 2;
          } else if (meta.endpoint_id) {
            endpointId = meta.endpoint_id;
          } else if (meta.endpoint) {
            endpointId = meta.endpoint;
          } else {
            endpointId = entity.endpointID || 1;
          }
        } else {
          endpointId = 1; // Fallback
        }
        const endpoint = meta.device.getEndpoint(endpointId);
        if (!endpoint) {
          throw new Error(`Endpoint ${endpointId} not found`);
        }
        // Utiliser la commande 'on' du cluster On/Off (l'attribut onOff est en lecture seule)
        await endpoint.command('genOnOff', 'on', {}, {disableDefaultResponse: true});
      },
    },
  ],
  configure: async (device, coordinatorEndpoint, logger) => {
    const ep1 = device.getEndpoint(1), ep2 = device.getEndpoint(2);
    // Bind les clusters On/Off
    await reporting.bind(ep1, coordinatorEndpoint, ['genOnOff']);
    await reporting.bind(ep2, coordinatorEndpoint, ['genOnOff']);
    await reporting.onOff(ep1);
    await reporting.onOff(ep2);
  },
}];
