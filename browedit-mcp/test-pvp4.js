import { BrowEditIpcClient } from './dist/client.js';

async function main() {
  const client = new BrowEditIpcClient('127.0.0.1', 38200);
  console.log('Connecting to BrowEdit3...');
  const ok = await client.connect();
  if (!ok) {
    console.error('Failed to connect to BrowEdit3 IPC!');
    process.exit(1);
  }
  console.log('Connected!');

  // 1. Ping
  console.log('1. Ping:', await client.send('ping'));

  // 2. Status
  const status = await client.send('get_status');
  console.log('2. Status:', status);

  // If arena_4 is not open, open it with 60s timeout
  if (!status.activeMap.includes('arena_4')) {
    console.log('Opening data\\arena_4.rsw (timeout: 60s)...');
    await client.send('open_map', { file: 'data\\arena_4.rsw' }, 60000);
  }

  // 3. Get Map Info
  console.log('3. Map Info:');
  const mapInfo = await client.send('get_map_info', {}, 30000);
  console.log(JSON.stringify(mapInfo, null, 2));

  // 4. List Objects & Models
  console.log('\n4. Objects in arena_4 (Sample of 15):');
  const objects = await client.send('list_objects', { type: 'all', limit: 15 });
  console.log(`Total objects on map: ${objects.totalFound}`);
  for (const obj of objects.objects) {
    console.log(` - [${obj.type}] "${obj.name}" ${obj.modelFile ? `model: ${obj.modelFile}` : ''} pos: [${obj.position?.map(n => Math.round(n)).join(', ')}]`);
  }

  // 5. Camera State
  console.log('\n5. Camera:');
  const camera = await client.send('get_camera');
  console.log(camera);

  // 6. Water
  console.log('\n6. Water properties:');
  const water = await client.send('get_water');
  console.log(water);

  // 7. Center Tile & GAT
  const cx = Math.floor(mapInfo.width / 2);
  const cy = Math.floor(mapInfo.height / 2);
  console.log(`\n7. Tile and GAT at center (${cx}, ${cy}):`);
  const tile = await client.send('get_tile', { x: cx, y: cy });
  console.log('Tile heights:', tile.heights);
  console.log('GAT cell:', tile.gatCells);

  // 8. Test live modification: Spawn a commemorative beacon/light in the center!
  console.log('\n8. Spawning a test Beacon/Light in the center of arena_4...');
  const spawnLight = await client.send('spawn_light', {
    name: 'PvP4_CenterBeacon',
    position: [cx * 10, tile.heights[0] + 15, cy * 10],
    color: [0.2, 0.8, 1.0], // cyan blue
    range: 120,
    intensity: 2.0
  });
  console.log('Beacon spawn result:', spawnLight);

  // 9. Focus camera on the new beacon!
  console.log('\n9. Focusing 3D Camera on the new beacon...');
  await client.send('set_camera', {
    center: [cx * 10, tile.heights[0], cy * 10],
    rotation: [35, 45],
    distance: 350
  });
  console.log('Camera focused!');

  client.disconnect();
  console.log('\n==========================================');
  console.log('PVP_4 TEST COMPLETED 100% SUCCESSFULLY!');
  console.log('==========================================');
  process.exit(0);
}

main().catch(err => {
  console.error('Test error:', err);
  process.exit(1);
});
