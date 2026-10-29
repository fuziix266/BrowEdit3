# BrowEdit3 Model Context Protocol (MCP) Server

Servidor oficial de **Model Context Protocol (MCP)** para **BrowEdit3**, el editor 3D de mapas de Ragnarok Online.

Permite a modelos de inteligencia artificial (Claude, Antigravity, Cursor, etc.) interactuar directamente con BrowEdit3 en tiempo real como un **Co-pilot de diseño de niveles y mapas**, permitiendo inspeccionar, crear, transformar y auditar mapas visualmente mientras el editor está en ejecución.

---

## 🚀 Características Principales

- **Conexión en Vivo con BrowEdit3:** Los cambios se renderizan instantáneamente en la ventana 3D del editor mediante un puente TCP IPC local de baja latencia (`127.0.0.1:38200`).
- **Historial Completo de Deshacer / Rehacer (Undo / Redo):** Cada operación ejecutada por la IA se integra con la pila nativa de acciones de BrowEdit, permitiendo revertir cualquier cambio con `Ctrl+Z`.
- **Manipulación de Entidades (RSW):**
  - Instanciación de modelos 3D (`.rsm`) con posición, rotación y escala.
  - Generación masiva (batch) de elementos (bosques, iluminación urbana, vallas).
  - Fuentes de luz puntuales dinámicas con color RGB, rango e intensidad.
  - Efectos visuales de partículas (`.str` / client-side effects).
  - Emisores de sonido ambiental (`.wav`).
  - Transformación y eliminación de objetos existentes.
- **Edición de Terreno y Malla 3D (GND):**
  - Lectura y modificación de elevaciones vértice a vértice (`h1`, `h2`, `h3`, `h4`).
  - Nivelación, elevación y hundimiento por áreas rectangulares.
  - Invalidación y refresco dinámico de mallas OpenGL (VBO/VAO dirty flags).
- **Control de Transitabilidad (GAT Grid):**
  - Inspección de tipos de celda: caminable (0), bloqueo (1), agua bloqueada (2), agua caminable (3), snipeable (4/5), acantilado (6).
  - Modificación puntual o rectangular de transitabilidad.
  - Cálculo automático de GAT basado en pendientes pronunciadas y cobertura de agua.
- **Cámara y Control del Visor:**
  - Inspección de la cámara 3D y teletransporte / enfoque a coordenadas y objetos específicos.
- **Gestión de Mapas:**
  - Crear mapas nuevos con dimensiones personalizadas.
  - Abrir mapas existentes (`.rsw`).
  - Guardar cambios.
  - Exportar minimapas de transitabilidad.

---

## 📦 Configuración en Clientes MCP

### 1. Claude Desktop (`claude_desktop_config.json`)
Añade la siguiente sección a tu archivo de configuración:

```json
{
  "mcpServers": {
    "browedit": {
      "command": "node",
      "args": [
        "C:/xampp_php8/htdocs/ragnarok/browedit-mcp/dist/index.js"
      ]
    }
  }
}
```

### 2. Antigravity / Gemini / Cursor
En la configuración de servidores MCP (`mcp_servers` o `settings.json`):

```json
{
  "browedit": {
    "command": "node",
    "args": ["C:/xampp_php8/htdocs/ragnarok/browedit-mcp/dist/index.js"]
  }
}
```

---

## 🛠️ Herramientas Disponibles

| Herramienta | Descripción |
| :--- | :--- |
| `browedit_ping` | Verifica la conectividad con BrowEdit3 y el puente MCP. |
| `browedit_get_status` | Devuelve mapas abiertos y mapa activo actual. |
| `browedit_get_map_info` | Resumen completo: dimensiones, cantidad de modelos, luces, efectos, sonidos y texturas. |
| `browedit_open_map` | Abre un archivo `.rsw` en el editor. |
| `browedit_new_map` | Crea un nuevo mapa en blanco con ancho y alto específicos. |
| `browedit_save_map` | Guarda el mapa activo. |
| `browedit_get_camera` / `set_camera` | Consulta o enfoca la cámara 3D en el visor. |
| `browedit_list_objects` | Lista objetos en el mapa filtrados por tipo o texto de búsqueda. |
| `browedit_get_object` | Obtiene propiedades detalladas de un objeto por nombre. |
| `browedit_spawn_model` | Coloca un modelo `.rsm` en $(X, Y, Z)$ con rotación y escala. |
| `browedit_spawn_light` | Crea una luz con color RGB, rango e intensidad. |
| `browedit_spawn_effect` | Instancia un efecto visual por su ID. |
| `browedit_spawn_sound` | Coloca un emisor de sonido ambiental. |
| `browedit_delete_object` | Elimina un objeto por nombre. |
| `browedit_transform_object` | Traslada, rota o escala un objeto. |
| `browedit_batch_spawn_models` | Instancia múltiples modelos en una sola transacción Undo. |
| `browedit_get_tile` | Consulta elevación GND, texturas y celdas GAT en $(X, Y)$. |
| `browedit_set_tile_height` | Modifica alturas de las 4 esquinas de un tile. |
| `browedit_set_area_height` | Nivela o desplaza la elevación en una región rectangular. |
| `browedit_get_gat` / `set_gat` | Consulta o modifica la transitabilidad GAT de celdas. |
| `browedit_auto_gat` | Calcula transitabilidad automáticamente por pendiente y agua. |
| `browedit_list_textures` | Lista de texturas cargadas en la paleta del mapa. |
| `browedit_get_water` / `set_water` | Consulta o ajusta el nivel y características del agua. |
| `browedit_undo` / `redo` | Deshace o rehace la última acción en BrowEdit3. |
| `browedit_export_minimap` | Exporta el minimapa de transitabilidad. |

---

## 🏗️ Compilación y Desarrollo

```bash
# Compilar TypeScript
cd browedit-mcp
npm run build

# Ejecutar tests de integración end-to-end contra BrowEdit3
node test-client.js
node test-models.js
```
