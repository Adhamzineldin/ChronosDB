import { useState, useEffect, useRef, useCallback } from 'react';
import { api } from '../api';
import type { ERTable } from '../types';

interface TablePosition {
  x: number;
  y: number;
}

export default function ERDiagram() {
  const [tables, setTables] = useState<ERTable[]>([]);
  const [positions, setPositions] = useState<Record<string, TablePosition>>({});
  const [loading, setLoading] = useState(true);
  const [dragging, setDragging] = useState<string | null>(null);
  const [dragOffset, setDragOffset] = useState({ x: 0, y: 0 });
  const svgRef = useRef<SVGSVGElement>(null);

  useEffect(() => {
    setLoading(true);
    api.getFullSchema().then(res => {
      if (res.success && res.tables) {
        setTables(res.tables);
        // Auto-layout tables in a grid
        const pos: Record<string, TablePosition> = {};
        const cols = Math.ceil(Math.sqrt(res.tables.length));
        res.tables.forEach((t: ERTable, i: number) => {
          const col = i % cols;
          const row = Math.floor(i / cols);
          pos[t.name] = { x: 50 + col * 280, y: 50 + row * 250 };
        });
        setPositions(pos);
      }
    }).finally(() => setLoading(false));
  }, []);

  const handleMouseDown = useCallback((tableName: string, e: React.MouseEvent) => {
    const pos = positions[tableName];
    if (!pos) return;
    setDragging(tableName);
    setDragOffset({ x: e.clientX - pos.x, y: e.clientY - pos.y });
  }, [positions]);

  const handleMouseMove = useCallback((e: React.MouseEvent) => {
    if (!dragging) return;
    setPositions(prev => ({
      ...prev,
      [dragging]: { x: e.clientX - dragOffset.x, y: e.clientY - dragOffset.y }
    }));
  }, [dragging, dragOffset]);

  const handleMouseUp = useCallback(() => {
    setDragging(null);
  }, []);

  if (loading) {
    return <div className="card"><div className="card-body"><p>Loading schema...</p></div></div>;
  }

  if (tables.length === 0) {
    return (
      <div className="card">
        <div className="card-body" style={{ textAlign: 'center', padding: '3rem' }}>
          <p style={{ color: '#9ca0b0' }}>No tables found. Create tables first to see the ER diagram.</p>
        </div>
      </div>
    );
  }

  // Collect FK relationships for drawing lines
  const relationships: { from: string; fromCol: string; to: string; toCol: string }[] = [];
  tables.forEach(t => {
    t.foreign_keys?.forEach(fk => {
      relationships.push({ from: t.name, fromCol: fk.column, to: fk.ref_table, toCol: fk.ref_column });
    });
  });

  const TABLE_WIDTH = 220;
  const ROW_HEIGHT = 22;
  const HEADER_HEIGHT = 32;

  return (
    <div className="er-diagram">
      <div className="card" style={{ marginBottom: '1rem' }}>
        <div className="card-header">
          <h3>Entity-Relationship Diagram</h3>
          <span style={{ color: '#9ca0b0', fontSize: '0.8rem', marginLeft: '1rem' }}>
            {tables.length} tables, {relationships.length} relationships | Drag tables to reposition
          </span>
        </div>
        <div className="card-body" style={{ padding: 0, overflow: 'auto', background: '#0d0f17' }}>
          <svg
            ref={svgRef}
            width="100%"
            height="700"
            style={{ minWidth: '800px', cursor: dragging ? 'grabbing' : 'default' }}
            onMouseMove={handleMouseMove}
            onMouseUp={handleMouseUp}
            onMouseLeave={handleMouseUp}
          >
            <defs>
              <marker id="arrowhead" markerWidth="10" markerHeight="7" refX="10" refY="3.5" orient="auto">
                <polygon points="0 0, 10 3.5, 0 7" fill="#6366f1" opacity="0.6" />
              </marker>
            </defs>

            {/* Relationship Lines */}
            {relationships.map((rel, i) => {
              const fromPos = positions[rel.from];
              const toPos = positions[rel.to];
              if (!fromPos || !toPos) return null;

              const fromTable = tables.find(t => t.name === rel.from);
              const fromColIdx = fromTable?.columns.findIndex(c => c.name === rel.fromCol) ?? 0;
              const toTable = tables.find(t => t.name === rel.to);
              const toColIdx = toTable?.columns.findIndex(c => c.name === rel.toCol) ?? 0;

              const x1 = fromPos.x + TABLE_WIDTH;
              const y1 = fromPos.y + HEADER_HEIGHT + fromColIdx * ROW_HEIGHT + ROW_HEIGHT / 2;
              const x2 = toPos.x;
              const y2 = toPos.y + HEADER_HEIGHT + toColIdx * ROW_HEIGHT + ROW_HEIGHT / 2;

              const midX = (x1 + x2) / 2;

              return (
                <g key={i}>
                  <path
                    d={`M ${x1} ${y1} C ${midX} ${y1}, ${midX} ${y2}, ${x2} ${y2}`}
                    fill="none" stroke="#6366f1" strokeWidth="2" opacity="0.5"
                    markerEnd="url(#arrowhead)"
                  />
                  <title>{rel.from}.{rel.fromCol} -> {rel.to}.{rel.toCol}</title>
                </g>
              );
            })}

            {/* Table Boxes */}
            {tables.map(table => {
              const pos = positions[table.name];
              if (!pos) return null;
              const tableHeight = HEADER_HEIGHT + table.columns.length * ROW_HEIGHT + 8;

              return (
                <g key={table.name} style={{ cursor: 'grab' }}
                  onMouseDown={(e) => handleMouseDown(table.name, e)}>
                  {/* Shadow */}
                  <rect x={pos.x + 3} y={pos.y + 3} width={TABLE_WIDTH} height={tableHeight}
                    rx="6" fill="#000" opacity="0.3" />
                  {/* Background */}
                  <rect x={pos.x} y={pos.y} width={TABLE_WIDTH} height={tableHeight}
                    rx="6" fill="#1e2130" stroke="#2a2d3e" strokeWidth="1" />
                  {/* Header */}
                  <rect x={pos.x} y={pos.y} width={TABLE_WIDTH} height={HEADER_HEIGHT}
                    rx="6" fill="#6366f1" />
                  <rect x={pos.x} y={pos.y + HEADER_HEIGHT - 6} width={TABLE_WIDTH} height={6}
                    fill="#6366f1" />
                  <text x={pos.x + TABLE_WIDTH / 2} y={pos.y + 21}
                    textAnchor="middle" fill="#fff" fontSize="13" fontWeight="700">
                    {table.name}
                  </text>

                  {/* Columns */}
                  {table.columns.map((col, ci) => {
                    const isPK = table.primary_keys?.includes(col.name);
                    const isFK = table.foreign_keys?.some(fk => fk.column === col.name);
                    const cy = pos.y + HEADER_HEIGHT + ci * ROW_HEIGHT + 16;

                    return (
                      <g key={ci}>
                        <text x={pos.x + 10} y={cy} fill={isPK ? '#f59e0b' : isFK ? '#818cf8' : '#9ca0b0'}
                          fontSize="11.5" fontWeight={isPK ? '600' : '400'}>
                          {isPK ? 'PK ' : isFK ? 'FK ' : ''}{col.name}
                        </text>
                        <text x={pos.x + TABLE_WIDTH - 10} y={cy} textAnchor="end"
                          fill="#6b6f82" fontSize="10.5">
                          {col.type}
                        </text>
                      </g>
                    );
                  })}
                </g>
              );
            })}
          </svg>
        </div>
      </div>

      {/* Legend */}
      <div className="card">
        <div className="card-body" style={{ display: 'flex', gap: '2rem', padding: '0.75rem 1rem' }}>
          <span style={{ display: 'flex', alignItems: 'center', gap: '0.5rem', fontSize: '0.85rem' }}>
            <span style={{ width: 12, height: 12, borderRadius: 2, background: '#f59e0b', display: 'inline-block' }} />
            <span style={{ color: '#9ca0b0' }}>Primary Key</span>
          </span>
          <span style={{ display: 'flex', alignItems: 'center', gap: '0.5rem', fontSize: '0.85rem' }}>
            <span style={{ width: 12, height: 12, borderRadius: 2, background: '#818cf8', display: 'inline-block' }} />
            <span style={{ color: '#9ca0b0' }}>Foreign Key</span>
          </span>
          <span style={{ display: 'flex', alignItems: 'center', gap: '0.5rem', fontSize: '0.85rem' }}>
            <span style={{ width: 12, height: 2, background: '#6366f1', display: 'inline-block' }} />
            <span style={{ color: '#9ca0b0' }}>Relationship</span>
          </span>
        </div>
      </div>
    </div>
  );
}
