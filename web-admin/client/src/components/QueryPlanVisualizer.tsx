import type { ChronosResult } from '../types';

interface Props {
  result: ChronosResult;
}

interface PlanNode {
  step: string;
  operation: string;
  details: string;
  estRows: string;
  actRows?: string;
  timeMs?: string;
  aiInsight: string;
}

function parseExplainResult(result: ChronosResult): PlanNode[] {
  if (!result.data?.rows) return [];
  const cols = result.data.columns;
  return result.data.rows
    .filter(row => row[0] !== 'Total')
    .map(row => {
      const node: PlanNode = {
        step: row[0] || '',
        operation: row[1] || '',
        details: '',
        estRows: '',
        aiInsight: '',
      };
      // EXPLAIN has: Step, Operation, Details, Est. Rows, AI Insight
      // EXPLAIN ANALYZE has: Step, Operation, Est. Rows, Act. Rows, Time (ms), AI Insight
      if (cols.includes('Act. Rows')) {
        node.estRows = row[2] || '';
        node.actRows = row[3] || '';
        node.timeMs = row[4] || '';
        node.aiInsight = row[5] || '';
      } else {
        node.details = row[2] || '';
        node.estRows = row[3] || '';
        node.aiInsight = row[4] || '';
      }
      return node;
    });
}

function getOpColor(op: string): string {
  if (op.includes('SEQ SCAN')) return '#f59e0b';
  if (op.includes('INDEX SCAN')) return '#10b981';
  if (op.includes('HASH SCAN')) return '#6366f1';
  if (op.includes('FILTER')) return '#818cf8';
  if (op.includes('JOIN')) return '#ec4899';
  if (op.includes('AGGREGATE')) return '#8b5cf6';
  if (op.includes('SORT')) return '#06b6d4';
  return '#9ca0b0';
}

function getTimingColor(timeMs: string): string {
  const t = parseFloat(timeMs);
  if (isNaN(t)) return '#9ca0b0';
  if (t < 1) return '#10b981';
  if (t < 10) return '#f59e0b';
  return '#ef4444';
}

export default function QueryPlanVisualizer({ result }: Props) {
  const nodes = parseExplainResult(result);
  const isAnalyze = result.data?.columns?.includes('Act. Rows');

  // Total row
  const totalRow = result.data?.rows?.find(r => r[0] === 'Total');

  if (nodes.length === 0) return null;

  const NODE_WIDTH = 280;
  const NODE_HEIGHT = isAnalyze ? 90 : 70;
  const V_GAP = 30;
  const svgWidth = NODE_WIDTH + 60;
  const svgHeight = nodes.length * (NODE_HEIGHT + V_GAP) + (totalRow ? 50 : 20);

  return (
    <div className="query-plan-viz" style={{ marginTop: '1rem' }}>
      <div className="card">
        <div className="card-header">
          <h3>Query Plan {isAnalyze ? '(ANALYZE)' : ''}</h3>
        </div>
        <div className="card-body" style={{ overflow: 'auto', background: '#0d0f17', padding: '1rem' }}>
          <svg width={svgWidth} height={svgHeight}>
            {nodes.map((node, i) => {
              const x = 30;
              const y = 10 + i * (NODE_HEIGHT + V_GAP);
              const color = getOpColor(node.operation);

              return (
                <g key={i}>
                  {/* Connector line to next node */}
                  {i < nodes.length - 1 && (
                    <line
                      x1={x + NODE_WIDTH / 2} y1={y + NODE_HEIGHT}
                      x2={x + NODE_WIDTH / 2} y2={y + NODE_HEIGHT + V_GAP}
                      stroke="#2a2d3e" strokeWidth="2" strokeDasharray="4"
                    />
                  )}

                  {/* Node box */}
                  <rect x={x} y={y} width={NODE_WIDTH} height={NODE_HEIGHT}
                    rx="8" fill="#1e2130" stroke={color} strokeWidth="2" />

                  {/* Color bar */}
                  <rect x={x} y={y} width={6} height={NODE_HEIGHT} rx="3"
                    fill={color} />

                  {/* Step number */}
                  <circle cx={x + 22} cy={y + 18} r="10" fill={color} opacity="0.2" />
                  <text x={x + 22} y={y + 22} textAnchor="middle" fill={color}
                    fontSize="11" fontWeight="700">{node.step}</text>

                  {/* Operation name */}
                  <text x={x + 42} y={y + 22} fill="#e4e6ef" fontSize="13" fontWeight="600">
                    {node.operation}
                  </text>

                  {/* Details / Est Rows */}
                  <text x={x + 16} y={y + 42} fill="#9ca0b0" fontSize="11">
                    {node.details ? `${node.details} | ` : ''}Est: {node.estRows} rows
                  </text>

                  {/* Analyze-specific info */}
                  {isAnalyze && node.actRows && (
                    <>
                      <text x={x + 16} y={y + 58} fill="#9ca0b0" fontSize="11">
                        Actual: {node.actRows} rows |{' '}
                        <tspan fill={getTimingColor(node.timeMs || '')}>{node.timeMs}ms</tspan>
                      </text>
                      {/* Timing bar */}
                      {node.timeMs && (
                        <rect x={x + 16} y={y + 65} width={Math.min(parseFloat(node.timeMs) * 20, NODE_WIDTH - 32)}
                          height={4} rx="2" fill={getTimingColor(node.timeMs)} opacity="0.6" />
                      )}
                    </>
                  )}

                  {/* AI Insight */}
                  {node.aiInsight && (
                    <text x={x + 16} y={y + (isAnalyze ? 82 : 58)} fill="#6366f1" fontSize="10">
                      {node.aiInsight}
                    </text>
                  )}
                </g>
              );
            })}

            {/* Total summary */}
            {totalRow && (
              <text x={30} y={svgHeight - 10} fill="#e4e6ef" fontSize="12" fontWeight="600">
                Total: {totalRow[3] || totalRow[2]} rows
                {totalRow[4] && ` in ${totalRow[4]}`}
              </text>
            )}
          </svg>
        </div>
      </div>
    </div>
  );
}
