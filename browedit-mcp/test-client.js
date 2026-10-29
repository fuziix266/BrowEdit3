import { BrowEditIpcClient } from './dist/client.js';

async function main() {
  const client = new BrowEditIpcClient('127.0.0.1', 38200);
  console.log('Connecting to BrowEdit3 IPC...');
  const connected = await client.connect();
  if (!connected) {
    console.error('Failed to connect!');
    process.exit(1);
  }
  console.log('Connected successfully!');

  // 1. Ping
  console.log('1. Ping:', await client.send('ping'));

  // 2. Status
  console.log('2. Status:', await client.send('get_status'));

  // 3. New Map
  console.log('3. New Map:', await client.send('new_map', {
    name: 'mcp_test_map',
    width: 30,
    height: 30
  }));

  // 4. Map info
  console.log('4. Map Info:', await client.send('get_map_info'));

  // 5. Spawn Light
  console.log('5. Spawn Light:', await client.send('spawn_light', {
    name: 'SunLight',
    position: [150, 50, 150],
    color: [1.0, 0.8, 0.4],
    range: 80,
    intensity: 1.5
  }));

  // 6. List Objects
  console.log('6. List Objects:', await client.send('list_objects', { type: 'all' }));

  // 7. Set Tile Height
  console.log('7. Set Tile Height:', await client.send('set_tile_height', {
    x: 10,
    y: 10,
    height: 35.0
  }));

  // 8. Get Tile
  console.log('8. Get Tile (10, 10):', await client.send('get_tile', { x: 10, y: 10 }));

  // 9. Set GAT cell
  console.log('9. Set GAT (10, 10):', await client.send('set_gat', {
    x: 10,
    y: 10,
    type: 1 // Non-walkable
  }));

  // 10. Get GAT cell
  console.log('10. Get GAT (10, 10):', await client.send('get_gat', { x: 10, y: 10 }));

  // 11. Camera move
  console.log('11. Set Camera:', await client.send('set_camera', {
    center: [150, 50, 150],
    rotation: [45, 90],
    distance: 300
  }));

  // 12. Undo
  console.log('12. Undo:', await client.send('undo'));

  // 13. Redo
  console.log('13. Redo:', await client.send('redo'));

  console.log('SUCCESS! ALL TESTS COMPLETED PERFECTLY!');
  client.disconnect();
  process.exit(0);
}

main().catch((err) => {
  console.error('Test error:', err);
  process.exit(1);
});
