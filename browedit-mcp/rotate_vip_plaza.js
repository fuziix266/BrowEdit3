import { BrowEditIpcClient } from './dist/client.js';

async function main() {
  const client = new BrowEditIpcClient('127.0.0.1', 38200);
  console.log('Conectando a BrowEdit3 IPC...');
  const ok = await client.connect();
  if (!ok) {
    console.error('Error al conectar con BrowEdit3 en 127.0.0.1:38200');
    process.exit(1);
  }
  console.log('Conectado con éxito.');

  // 1. Abrir vip_lounge si no está abierto
  const status = await client.send('get_status');
  if (!status.activeMap.includes('vip_lounge')) {
    console.log('Cargando mapa data\\vip_lounge.rsw...');
    await client.send('open_map', { file: 'data\\vip_lounge.rsw' }, 60000);
  }
  console.log('Mapa vip_lounge activo en BrowEdit3.');

  // 2. Obtener info inicial
  const info = await client.send('get_map_info', {}, 30000);
  console.log(`Dimensiones: GND ${info.width}x${info.height}, GAT ${info.gatWidth}x${info.gatHeight}, Total objetos: ${info.objectsTotal}`);

  // 3. Ejecutar la rotación de 180° de la plaza completa
  console.log('\n======================================================');
  console.log('Rotando la plaza 180° (modelos 3D, luces, terreno GND, texturas y GAT)...');
  console.log('======================================================');
  
  const rotateResult = await client.send('rotate_area', {
    gndMinX: 36,
    gndMaxX: 99,
    gndMinY: 95,
    gndMaxY: 158,
    rswCenterX: -105.0,
    rswCenterZ: 295.0,
    rotateObjects: true,
    rotateGnd: true,
    rotateGat: true,
    autoSave: true
  }, 60000);

  console.log('Resultado de la rotación:', rotateResult);

  // 4. Enfocar cámara 3D sobre la plaza
  console.log('\nEnfocando cámara 3D de BrowEdit sobre la plaza rotada...');
  await client.send('set_camera', {
    center: [-105.0, -14.0, 295.0],
    rotation: [45, 0],
    distance: 650
  });
  console.log('Cámara enfocada.');

  // 5. Verificar la posición del edificio principal (Object873) tras rotar
  console.log('\nVerificando posición del edificio principal con techo azul (Object873)...');
  const objs = await client.send('list_objects', { type: 'model', limit: 1000 });
  const mansion = objs.objects.find(o => o.name === 'Object873');
  if (mansion) {
    console.log(`Edificio principal: ${mansion.name} (${mansion.modelFile})`);
    console.log(` - Nueva Posición: [${mansion.position.map(n => Math.round(n * 10) / 10).join(', ')}]`);
    console.log(` - Nueva Rotación Y: ${Math.round(mansion.rotation[1])}°`);
  }

  client.disconnect();
  console.log('\n======================================================');
  console.log('ROTACIÓN DE 180° DE LA PLAZA COMPLETADA EXITOSAMENTE');
  console.log('======================================================');
  process.exit(0);
}

main().catch(err => {
  console.error('Error durante la rotación:', err);
  process.exit(1);
});
