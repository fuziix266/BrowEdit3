import { BrowEditIpcClient } from './dist/client.js';

async function main() {
  const client = new BrowEditIpcClient('127.0.0.1', 38200);
  await client.connect();

  console.log('Testing model spawning...');
  // Check if we can find any model file in data/model/
  // Spawn a model
  const spawnRes = await client.send('spawn_model', {
    model: 'data\\model\\prontera\\tree.rsm',
    position: [120, 0, 120],
    rotation: [0, 45, 0],
    scale: [1.2, 1.2, 1.2],
    name: 'PronteraTree'
  });
  console.log('Spawn Model Result:', spawnRes);

  // Batch spawn
  const batchRes = await client.send('batch_spawn_models', {
    models: [
      {
        model: 'data\\model\\prontera\\tree.rsm',
        position: [100, 0, 100],
        rotation: [0, 15, 0],
        scale: [1, 1, 1],
        name: 'Tree_1'
      },
      {
        model: 'data\\model\\prontera\\tree.rsm',
        position: [140, 0, 100],
        rotation: [0, 60, 0],
        scale: [0.9, 0.9, 0.9],
        name: 'Tree_2'
      },
      {
        model: 'data\\model\\prontera\\tree.rsm',
        position: [120, 0, 140],
        rotation: [0, 120, 0],
        scale: [1.1, 1.1, 1.1],
        name: 'Tree_3'
      }
    ]
  });
  console.log('Batch Spawn Result:', batchRes);

  const objects = await client.send('list_objects', { type: 'model' });
  console.log('Total Models in Map:', objects.totalFound, objects.objects.map(o => o.name));

  // Transform object
  const transformRes = await client.send('transform_object', {
    name: 'PronteraTree',
    position: [125, 0, 125],
    scale: [1.5, 1.5, 1.5]
  });
  console.log('Transform Result:', transformRes);

  // Auto GAT calculation
  const autoGatRes = await client.send('auto_gat', { maxSlope: 15.0 });
  console.log('Auto GAT Result:', autoGatRes);

  // Water check
  const water = await client.send('get_water');
  console.log('Water Info:', water);

  client.disconnect();
  console.log('MODEL AND ADVANCED TESTS PASSED!');
  process.exit(0);
}

main().catch(err => {
  console.error('Error:', err);
  process.exit(1);
});
