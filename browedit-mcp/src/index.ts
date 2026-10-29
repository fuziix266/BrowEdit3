#!/usr/bin/env node

import { Server } from '@modelcontextprotocol/sdk/server/index.js';
import { StdioServerTransport } from '@modelcontextprotocol/sdk/server/stdio.js';
import {
  CallToolRequestSchema,
  ListToolsRequestSchema,
  ListResourcesRequestSchema,
  ReadResourceRequestSchema,
} from '@modelcontextprotocol/sdk/types.js';
import { BrowEditIpcClient } from './client.js';

const client = new BrowEditIpcClient('127.0.0.1', 38200);

const server = new Server(
  {
    name: 'browedit-mcp',
    version: '1.0.0',
  },
  {
    capabilities: {
      tools: {},
      resources: {},
    },
  }
);

// Define tools
server.setRequestHandler(ListToolsRequestSchema, async () => {
  return {
    tools: [
      {
        name: 'browedit_ping',
        description: 'Verifica la conexión con la aplicación BrowEdit3 y el puente MCP.',
        inputSchema: {
          type: 'object',
          properties: {},
        },
      },
      {
        name: 'browedit_get_status',
        description: 'Obtiene el estado general de BrowEdit3: mapa activo y lista de mapas abiertos.',
        inputSchema: {
          type: 'object',
          properties: {},
        },
      },
      {
        name: 'browedit_get_map_info',
        description: 'Obtiene información detallada del mapa activo: dimensiones, texturas, conteo de objetos, agua, etc.',
        inputSchema: {
          type: 'object',
          properties: {},
        },
      },
      {
        name: 'browedit_open_map',
        description: 'Abre un mapa existente en BrowEdit3 dado su path (ej. "data\\prontera.rsw").',
        inputSchema: {
          type: 'object',
          properties: {
            file: {
              type: 'string',
              description: 'Ruta del archivo de mapa .rsw (ej: "data\\prontera.rsw" o ruta local)',
            },
          },
          required: ['file'],
        },
      },
      {
        name: 'browedit_new_map',
        description: 'Crea un mapa nuevo con las dimensiones indicadas.',
        inputSchema: {
          type: 'object',
          properties: {
            name: { type: 'string', description: 'Nombre del mapa nuevo (ej: "new_town")' },
            width: { type: 'number', description: 'Ancho en tiles (ej: 100)' },
            height: { type: 'number', description: 'Alto en tiles (ej: 100)' },
          },
          required: ['name', 'width', 'height'],
        },
      },
      {
        name: 'browedit_save_map',
        description: 'Guarda el mapa activo actual en disco.',
        inputSchema: {
          type: 'object',
          properties: {},
        },
      },
      {
        name: 'browedit_get_camera',
        description: 'Obtiene la posición objetivo, rotación y distancia de la cámara 3D en el visor de BrowEdit.',
        inputSchema: {
          type: 'object',
          properties: {},
        },
      },
      {
        name: 'browedit_set_camera',
        description: 'Mueve y enfoca la cámara 3D de BrowEdit en coordenadas específicas.',
        inputSchema: {
          type: 'object',
          properties: {
            center: {
              type: 'array',
              items: { type: 'number' },
              description: 'Coordenadas [x, y, z] hacia donde apunta la cámara',
            },
            rotation: {
              type: 'array',
              items: { type: 'number' },
              description: 'Rotación [pitch, yaw] en grados',
            },
            distance: {
              type: 'number',
              description: 'Distancia de la cámara al objetivo',
            },
          },
        },
      },
      {
        name: 'browedit_list_objects',
        description: 'Lista los objetos presentes en el mapa con soporte de filtros por tipo y búsqueda.',
        inputSchema: {
          type: 'object',
          properties: {
            type: {
              type: 'string',
              enum: ['all', 'model', 'light', 'sound', 'effect'],
              description: 'Filtrar por tipo de objeto (por defecto "all")',
            },
            search: {
              type: 'string',
              description: 'Cadena de texto para buscar por nombre o nombre de archivo del modelo',
            },
            limit: {
              type: 'number',
              description: 'Límite de resultados a devolver (por defecto 100)',
            },
            offset: {
              type: 'number',
              description: 'Desplazamiento para paginación (por defecto 0)',
            },
          },
        },
      },
      {
        name: 'browedit_get_object',
        description: 'Obtiene los detalles completos de un objeto del mapa por su nombre.',
        inputSchema: {
          type: 'object',
          properties: {
            name: { type: 'string', description: 'Nombre único del nodo/objeto en el mapa' },
          },
          required: ['name'],
        },
      },
      {
        name: 'browedit_spawn_model',
        description: 'Instancia un modelo 3D (.rsm) en las coordenadas especificadas con rotación y escala.',
        inputSchema: {
          type: 'object',
          properties: {
            model: {
              type: 'string',
              description: 'Ruta relativa del modelo .rsm (ej: "data\\model\\prontera\\tree_01.rsm")',
            },
            position: {
              type: 'array',
              items: { type: 'number' },
              description: 'Coordenadas [x, y, z] donde colocar el modelo',
            },
            rotation: {
              type: 'array',
              items: { type: 'number' },
              description: 'Rotación [rx, ry, rz] en grados (opcional)',
            },
            scale: {
              type: 'array',
              items: { type: 'number' },
              description: 'Escala [sx, sy, sz] (opcional, por defecto [1, 1, 1])',
            },
            name: {
              type: 'string',
              description: 'Nombre personalizado para el objeto (opcional)',
            },
          },
          required: ['model', 'position'],
        },
      },
      {
        name: 'browedit_spawn_light',
        description: 'Crea una fuente de luz puntual con color RGB, rango e intensidad.',
        inputSchema: {
          type: 'object',
          properties: {
            position: {
              type: 'array',
              items: { type: 'number' },
              description: 'Coordenadas [x, y, z] de la luz',
            },
            color: {
              type: 'array',
              items: { type: 'number' },
              description: 'Color RGB normalizado [r, g, b] (valores 0.0 a 1.0)',
            },
            range: {
              type: 'number',
              description: 'Radio de alcance de la luz (por defecto 50)',
            },
            intensity: {
              type: 'number',
              description: 'Intensidad de la luz (por defecto 1.0)',
            },
            name: {
              type: 'string',
              description: 'Nombre de la luz (opcional)',
            },
          },
          required: ['position'],
        },
      },
      {
        name: 'browedit_spawn_effect',
        description: 'Crea un efecto visual en el mapa por su ID de efecto de Ragnarok.',
        inputSchema: {
          type: 'object',
          properties: {
            position: {
              type: 'array',
              items: { type: 'number' },
              description: 'Coordenadas [x, y, z] del efecto',
            },
            effectId: {
              type: 'number',
              description: 'ID numérico del efecto (ej: humo, fuego, etc.)',
            },
            name: {
              type: 'string',
              description: 'Nombre del efecto (opcional)',
            },
          },
          required: ['position', 'effectId'],
        },
      },
      {
        name: 'browedit_spawn_sound',
        description: 'Coloca un emisor de sonido ambiental en el mapa.',
        inputSchema: {
          type: 'object',
          properties: {
            position: {
              type: 'array',
              items: { type: 'number' },
              description: 'Coordenadas [x, y, z] del sonido',
            },
            file: {
              type: 'string',
              description: 'Ruta del archivo de sonido .wav (ej: "data\\wav\\water.wav")',
            },
            volume: {
              type: 'number',
              description: 'Volumen (0.0 a 1.0)',
            },
            range: {
              type: 'number',
              description: 'Rango de audición en celdas',
            },
            cycle: {
              type: 'number',
              description: 'Ciclo de repetición en segundos',
            },
            name: {
              type: 'string',
              description: 'Nombre del objeto de sonido (opcional)',
            },
          },
          required: ['position', 'file'],
        },
      },
      {
        name: 'browedit_delete_object',
        description: 'Elimina un objeto del mapa por su nombre de nodo.',
        inputSchema: {
          type: 'object',
          properties: {
            name: { type: 'string', description: 'Nombre del objeto a eliminar' },
          },
          required: ['name'],
        },
      },
      {
        name: 'browedit_transform_object',
        description: 'Modifica la posición, rotación o escala de un objeto existente.',
        inputSchema: {
          type: 'object',
          properties: {
            name: { type: 'string', description: 'Nombre del objeto' },
            position: { type: 'array', items: { type: 'number' }, description: 'Nueva posición [x, y, z]' },
            rotation: { type: 'array', items: { type: 'number' }, description: 'Nueva rotación [rx, ry, rz]' },
            scale: { type: 'array', items: { type: 'number' }, description: 'Nueva escala [sx, sy, sz]' },
          },
          required: ['name'],
        },
      },
      {
        name: 'browedit_batch_spawn_models',
        description: 'Instancia múltiples modelos 3D en una sola transacción atómica de Undo/Redo (ideal para bosques, hileras de luces, etc.).',
        inputSchema: {
          type: 'object',
          properties: {
            models: {
              type: 'array',
              items: {
                type: 'object',
                properties: {
                  model: { type: 'string' },
                  position: { type: 'array', items: { type: 'number' } },
                  rotation: { type: 'array', items: { type: 'number' } },
                  scale: { type: 'array', items: { type: 'number' } },
                  name: { type: 'string' },
                },
                required: ['model', 'position'],
              },
              description: 'Lista de especificaciones de modelos a colocar',
            },
          },
          required: ['models'],
        },
      },
      {
        name: 'browedit_get_tile',
        description: 'Consulta elevación, texturas y datos GAT de una celda en coordenadas (x, y).',
        inputSchema: {
          type: 'object',
          properties: {
            x: { type: 'number', description: 'Coordenada X del tile' },
            y: { type: 'number', description: 'Coordenada Y del tile' },
          },
          required: ['x', 'y'],
        },
      },
      {
        name: 'browedit_set_tile_height',
        description: 'Establece la elevación del terreno en una celda (altura fija o 4 esquinas h1, h2, h3, h4).',
        inputSchema: {
          type: 'object',
          properties: {
            x: { type: 'number', description: 'Coordenada X del tile' },
            y: { type: 'number', description: 'Coordenada Y del tile' },
            height: { type: 'number', description: 'Altura uniforme para las 4 esquinas' },
            heights: {
              type: 'array',
              items: { type: 'number' },
              description: 'Alturas individuales para cada esquina [h1, h2, h3, h4]',
            },
          },
          required: ['x', 'y'],
        },
      },
      {
        name: 'browedit_set_area_height',
        description: 'Modifica la elevación de un área rectangular (nivelar, sumar altura o suavizar).',
        inputSchema: {
          type: 'object',
          properties: {
            minX: { type: 'number', description: 'X inicial' },
            minY: { type: 'number', description: 'Y inicial' },
            maxX: { type: 'number', description: 'X final' },
            maxY: { type: 'number', description: 'Y final' },
            mode: {
              type: 'string',
              enum: ['flatten', 'offset'],
              description: '"flatten" (fija a un valor) o "offset" (suma/resta altura)',
            },
            value: { type: 'number', description: 'Valor de altura o desplazamiento' },
          },
          required: ['minX', 'minY', 'maxX', 'maxY', 'mode', 'value'],
        },
      },
      {
        name: 'browedit_get_gat',
        description: 'Consulta el tipo de transitabilidad GAT de una celda (0=caminable, 1=bloqueo, 2=agua bloqueada, etc.).',
        inputSchema: {
          type: 'object',
          properties: {
            x: { type: 'number', description: 'Coordenada X en la grilla GAT' },
            y: { type: 'number', description: 'Coordenada Y en la grilla GAT' },
          },
          required: ['x', 'y'],
        },
      },
      {
        name: 'browedit_set_gat',
        description: 'Establece el tipo de celda GAT en una posición individual o un área rectangular.',
        inputSchema: {
          type: 'object',
          properties: {
            x: { type: 'number', description: 'Coordenada X (para celda individual)' },
            y: { type: 'number', description: 'Coordenada Y (para celda individual)' },
            minX: { type: 'number', description: 'X inicial (para área)' },
            minY: { type: 'number', description: 'Y inicial (para área)' },
            maxX: { type: 'number', description: 'X final (para área)' },
            maxY: { type: 'number', description: 'Y final (para área)' },
            type: {
              type: 'number',
              description: 'Tipo de transitabilidad: 0=Caminable, 1=Bloqueado, 2=Agua bloqueada, 3=Agua caminable, 4=Bloqueado disparable/snipeable, 5=Snipeable, 6=Acantilado',
            },
          },
          required: ['type'],
        },
      },
      {
        name: 'browedit_auto_gat',
        description: 'Calcula y asigna automáticamente celdas de bloqueo GAT basándose en la inclinación del terreno y el nivel del agua.',
        inputSchema: {
          type: 'object',
          properties: {
            maxSlope: {
              type: 'number',
              description: 'Diferencia máxima de altura permitida antes de considerar un acantilado (por defecto 20.0)',
            },
            waterBlocked: {
              type: 'boolean',
              description: 'Si es true, las celdas cubiertas por agua se marcan como no caminables (por defecto true)',
            },
          },
        },
      },
      {
        name: 'browedit_rotate_area',
        description: 'Rota una región rectangular del mapa (modelos 3D, luces, terreno GND, texturas y celdas GAT) en 180 grados alrededor de su centro simétrico.',
        inputSchema: {
          type: 'object',
          properties: {
            gndMinX: { type: 'number', description: 'Coordenada GND X mínima del área a rotar' },
            gndMaxX: { type: 'number', description: 'Coordenada GND X máxima del área a rotar' },
            gndMinY: { type: 'number', description: 'Coordenada GND Y mínima del área a rotar' },
            gndMaxY: { type: 'number', description: 'Coordenada GND Y máxima del área a rotar' },
            rswCenterX: { type: 'number', description: 'Coordenada X central en espacio RSW' },
            rswCenterZ: { type: 'number', description: 'Coordenada Z central en espacio RSW' },
            rotateObjects: { type: 'boolean', description: 'Si debe rotar los modelos 3D y luces (por defecto true)' },
            rotateGnd: { type: 'boolean', description: 'Si debe rotar los tiles y alturas GND (por defecto true)' },
            rotateGat: { type: 'boolean', description: 'Si debe rotar las celdas GAT (por defecto true)' },
            autoSave: { type: 'boolean', description: 'Si debe guardar el mapa automáticamente tras rotar (por defecto false)' },
          },
        },
      },
      {
        name: 'browedit_list_textures',
        description: 'Lista todas las texturas cargadas en la paleta del mapa GND.',
        inputSchema: {
          type: 'object',
          properties: {},
        },
      },
      {
        name: 'browedit_get_water',
        description: 'Obtiene las propiedades del agua del mapa activo (altura, tipo, amplitud, velocidad de ola).',
        inputSchema: {
          type: 'object',
          properties: {},
        },
      },
      {
        name: 'browedit_set_water',
        description: 'Configura la altura y propiedades del agua del mapa activo.',
        inputSchema: {
          type: 'object',
          properties: {
            height: { type: 'number', description: 'Nivel del agua' },
            type: { type: 'number', description: 'Tipo de textura/animación de agua' },
          },
        },
      },
      {
        name: 'browedit_undo',
        description: 'Deshace la última acción realizada en BrowEdit3.',
        inputSchema: {
          type: 'object',
          properties: {},
        },
      },
      {
        name: 'browedit_redo',
        description: 'Rehace la última acción deshecha en BrowEdit3.',
        inputSchema: {
          type: 'object',
          properties: {},
        },
      },
      {
        name: 'browedit_export_minimap',
        description: 'Genera y exporta el minimapa de transitabilidad del mapa activo.',
        inputSchema: {
          type: 'object',
          properties: {},
        },
      },
    ],
  };
});

// Execute tool requests
server.setRequestHandler(CallToolRequestSchema, async (request) => {
  const { name, arguments: args } = request.params;

  try {
    const rawMethod = name.replace(/^browedit_/, '');
    const result = await client.send(rawMethod, args || {});

    return {
      content: [
        {
          type: 'text',
          text: JSON.stringify(result, null, 2),
        },
      ],
    };
  } catch (error: any) {
    return {
      isError: true,
      content: [
        {
          type: 'text',
          text: `Error al ejecutar ${name}: ${error.message || String(error)}`,
        },
      ],
    };
  }
});

// Provide Resources
server.setRequestHandler(ListResourcesRequestSchema, async () => {
  return {
    resources: [
      {
        uri: 'browedit://active-map/summary',
        name: 'Resumen del Mapa Activo',
        description: 'Estadísticas e información general del mapa cargado actualmente en BrowEdit3.',
        mimeType: 'application/json',
      },
      {
        uri: 'browedit://active-map/textures',
        name: 'Texturas del Mapa Activo',
        description: 'Listado de texturas asociadas al mapa GND actual.',
        mimeType: 'application/json',
      },
    ],
  };
});

server.setRequestHandler(ReadResourceRequestSchema, async (request) => {
  const { uri } = request.params;

  if (uri === 'browedit://active-map/summary') {
    const info = await client.send('get_map_info', {});
    return {
      contents: [
        {
          uri,
          mimeType: 'application/json',
          text: JSON.stringify(info, null, 2),
        },
      ],
    };
  }

  if (uri === 'browedit://active-map/textures') {
    const textures = await client.send('list_textures', {});
    return {
      contents: [
        {
          uri,
          mimeType: 'application/json',
          text: JSON.stringify(textures, null, 2),
        },
      ],
    };
  }

  throw new Error(`Recurso no encontrado: ${uri}`);
});

// Start MCP Server
async function main() {
  const transport = new StdioServerTransport();
  await server.connect(transport);
  console.error('BrowEdit3 MCP Server iniciado en transporte stdio.');
}

main().catch((err) => {
  console.error('Error fatal al iniciar BrowEdit3 MCP Server:', err);
  process.exit(1);
});
