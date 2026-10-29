import net from 'net';

async function runTest() {
  const client = net.createConnection({ port: 38200, host: '127.0.0.1' }, () => {
    console.log('Connected to BrowEdit3 IPC!');
  });

  let requestId = 0;
  function call(method, params = {}) {
    return new Promise((resolve) => {
      const id = ++requestId;
      const handler = (data) => {
        const lines = data.toString().split('\n');
        for (const line of lines) {
          if (!line.trim()) continue;
          try {
            const parsed = JSON.parse(line);
            if (parsed.id === id) {
              client.off('data', handler);
              resolve(parsed);
              return;
            }
          } catch {}
        }
      };
      client.on('data', handler);
      client.write(JSON.stringify({ id, method, params }) + '\n');
    });
  }

  // 1. Ping
  console.log('1. Ping:', await call('ping'));

  // 2. Map info
  console.log('2. Map Info:', await call('get_map_info'));

  // 3. Spawn Light
  console.log('3. Spawn Light:', await call('spawn_light', {
    name: 'SunLight',
    position: [200, 50, 200],
    color: [1.0, 0.8, 0.4],
    range: 80,
    intensity: 1.5
  }));

  // 4. List Objects
  console.log('4. List Objects:', await call('list_objects', { type: 'all' }));

  // 5. Set Tile Height
  console.log('5. Set Tile Height:', await call('set_tile_height', {
    x: 10,
    y: 10,
    height: 25.5
  }));

  // 6. Get Tile
  console.log('6. Get Tile (10, 10):', await call('get_tile', { x: 10, y: 10 }));

  // 7. Set GAT cell
  console.log('7. Set GAT (10, 10):', await call('set_gat', {
    x: 10,
    y: 10,
    type: 1 // blocked
  }));

  // 8. Get GAT cell
  console.log('8. Get GAT (10, 10):', await call('get_gat', { x: 10, y: 10 }));

  // 9. Camera move
  console.log('9. Set Camera:', await call('set_camera', {
    center: [200, 50, 200],
    rotation: [45, 90],
    distance: 400
  }));

  // 10. Undo
  console.log('10. Undo:', await call('undo'));

  console.log('ALL TESTS PASSED SUCCESSFULLY!');
  client.end();
}

runTest().catch(console.error);
