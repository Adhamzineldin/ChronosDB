import { useState, useEffect } from 'react';
import { api } from '../api';
import type { ChronosResult } from '../types';

interface WhereClause {
  column: string;
  operator: string;
  value: string;
}

interface OrderByClause {
  column: string;
  direction: 'ASC' | 'DESC';
}

interface Props {
  currentDb: string;
}

export default function QueryBuilder({ currentDb }: Props) {
  const [tables, setTables] = useState<string[]>([]);
  const [columns, setColumns] = useState<string[]>([]);
  const [selectedTable, setSelectedTable] = useState('');
  const [selectedColumns, setSelectedColumns] = useState<string[]>([]);
  const [whereClauses, setWhereClauses] = useState<WhereClause[]>([]);
  const [orderByClauses, setOrderByClauses] = useState<OrderByClause[]>([]);
  const [limit, setLimit] = useState('');
  const [offset, setOffset] = useState('');
  const [generatedSQL, setGeneratedSQL] = useState('');
  const [result, setResult] = useState<ChronosResult | null>(null);
  const [executing, setExecuting] = useState(false);

  // Join state
  const [joinTable, setJoinTable] = useState('');
  const [joinType, setJoinType] = useState('INNER');
  const [joinCondition, setJoinCondition] = useState('');
  const [useJoin, setUseJoin] = useState(false);

  // Aggregate
  const [aggregate, setAggregate] = useState('');
  const [aggregateCol, setAggregateCol] = useState('');
  const [groupBy, setGroupBy] = useState('');

  useEffect(() => {
    api.getTables().then((res: any) => {
      if (res.data?.rows) {
        setTables(res.data.rows.map((r: string[]) => r[0]).filter(Boolean));
      }
    });
  }, [currentDb]);

  useEffect(() => {
    if (selectedTable) {
      api.getTableSchema(selectedTable).then((res: any) => {
        if (res.data?.rows) {
          setColumns(res.data.rows.map((r: string[]) => r[0]).filter(Boolean));
        }
      });
    } else {
      setColumns([]);
    }
    setSelectedColumns([]);
    setWhereClauses([]);
    setOrderByClauses([]);
  }, [selectedTable]);

  // Generate SQL whenever state changes
  useEffect(() => {
    if (!selectedTable) { setGeneratedSQL(''); return; }

    let sql = 'SELECT ';

    // Aggregate or columns
    if (aggregate && aggregateCol) {
      sql += `${aggregate}(${aggregateCol})`;
      if (groupBy) sql += `, ${groupBy}`;
    } else if (selectedColumns.length > 0) {
      sql += selectedColumns.join(', ');
    } else {
      sql += '*';
    }

    sql += ` FROM ${selectedTable}`;

    // Join
    if (useJoin && joinTable && joinCondition) {
      sql += ` ${joinType} JOIN ${joinTable} ON ${joinCondition}`;
    }

    // Where
    const validWheres = whereClauses.filter(w => w.column && w.operator && w.value);
    if (validWheres.length > 0) {
      sql += ' WHERE ';
      sql += validWheres.map(w => {
        const val = isNaN(Number(w.value)) ? `'${w.value}'` : w.value;
        return `${w.column} ${w.operator} ${val}`;
      }).join(' AND ');
    }

    // Group By
    if (aggregate && groupBy) {
      sql += ` GROUP BY ${groupBy}`;
    }

    // Order By
    const validOrders = orderByClauses.filter(o => o.column);
    if (validOrders.length > 0) {
      sql += ' ORDER BY ' + validOrders.map(o => `${o.column} ${o.direction}`).join(', ');
    }

    // Limit / Offset
    if (limit) sql += ` LIMIT ${limit}`;
    if (offset) sql += ` OFFSET ${offset}`;

    sql += ';';
    setGeneratedSQL(sql);
  }, [selectedTable, selectedColumns, whereClauses, orderByClauses, limit, offset,
      useJoin, joinTable, joinType, joinCondition, aggregate, aggregateCol, groupBy]);

  const executeQuery = async () => {
    if (!generatedSQL) return;
    setExecuting(true);
    try {
      const res = await api.executeQuery(generatedSQL);
      setResult(res);
    } catch (e: any) {
      setResult({ success: false, error: e.message });
    }
    setExecuting(false);
  };

  const toggleColumn = (col: string) => {
    setSelectedColumns(prev =>
      prev.includes(col) ? prev.filter(c => c !== col) : [...prev, col]
    );
  };

  return (
    <div className="query-builder">
      <div className="card" style={{ marginBottom: '1rem' }}>
        <div className="card-header"><h3>Visual Query Builder</h3></div>
        <div className="card-body">
          {/* Table Selection */}
          <div style={{ marginBottom: '1rem' }}>
            <label className="form-label">Table</label>
            <select className="form-select" value={selectedTable} onChange={e => setSelectedTable(e.target.value)}>
              <option value="">-- Select Table --</option>
              {tables.map(t => <option key={t} value={t}>{t}</option>)}
            </select>
          </div>

          {/* Column Selection */}
          {columns.length > 0 && (
            <div style={{ marginBottom: '1rem' }}>
              <label className="form-label">Columns (leave empty for *)</label>
              <div style={{ display: 'flex', flexWrap: 'wrap', gap: '0.5rem' }}>
                {columns.map(col => (
                  <label key={col} style={{ display: 'flex', alignItems: 'center', gap: '0.25rem',
                    padding: '0.25rem 0.5rem', background: selectedColumns.includes(col) ? '#6366f1' : '#1e2130',
                    borderRadius: '4px', cursor: 'pointer', color: selectedColumns.includes(col) ? '#fff' : '#9ca0b0',
                    fontSize: '0.85rem' }}>
                    <input type="checkbox" checked={selectedColumns.includes(col)}
                      onChange={() => toggleColumn(col)} style={{ display: 'none' }} />
                    {col}
                  </label>
                ))}
              </div>
            </div>
          )}

          {/* Aggregate */}
          <div style={{ marginBottom: '1rem', display: 'flex', gap: '0.5rem', alignItems: 'end' }}>
            <div>
              <label className="form-label">Aggregate</label>
              <select className="form-select" value={aggregate} onChange={e => setAggregate(e.target.value)} style={{ width: '120px' }}>
                <option value="">None</option>
                <option value="COUNT">COUNT</option>
                <option value="SUM">SUM</option>
                <option value="AVG">AVG</option>
                <option value="MIN">MIN</option>
                <option value="MAX">MAX</option>
              </select>
            </div>
            {aggregate && (
              <>
                <div>
                  <label className="form-label">Column</label>
                  <select className="form-select" value={aggregateCol} onChange={e => setAggregateCol(e.target.value)} style={{ width: '150px' }}>
                    <option value="*">*</option>
                    {columns.map(c => <option key={c} value={c}>{c}</option>)}
                  </select>
                </div>
                <div>
                  <label className="form-label">Group By</label>
                  <select className="form-select" value={groupBy} onChange={e => setGroupBy(e.target.value)} style={{ width: '150px' }}>
                    <option value="">None</option>
                    {columns.map(c => <option key={c} value={c}>{c}</option>)}
                  </select>
                </div>
              </>
            )}
          </div>

          {/* JOIN */}
          <div style={{ marginBottom: '1rem' }}>
            <label style={{ display: 'flex', alignItems: 'center', gap: '0.5rem', cursor: 'pointer' }}>
              <input type="checkbox" checked={useJoin} onChange={e => setUseJoin(e.target.checked)} />
              <span className="form-label" style={{ margin: 0 }}>JOIN</span>
            </label>
            {useJoin && (
              <div style={{ display: 'flex', gap: '0.5rem', marginTop: '0.5rem' }}>
                <select className="form-select" value={joinType} onChange={e => setJoinType(e.target.value)} style={{ width: '120px' }}>
                  <option>INNER</option><option>LEFT</option><option>RIGHT</option>
                </select>
                <select className="form-select" value={joinTable} onChange={e => setJoinTable(e.target.value)} style={{ width: '150px' }}>
                  <option value="">Table...</option>
                  {tables.filter(t => t !== selectedTable).map(t => <option key={t} value={t}>{t}</option>)}
                </select>
                <input className="form-input" placeholder="ON condition (e.g. t1.id = t2.id)"
                  value={joinCondition} onChange={e => setJoinCondition(e.target.value)} style={{ flex: 1 }} />
              </div>
            )}
          </div>

          {/* WHERE Clauses */}
          <div style={{ marginBottom: '1rem' }}>
            <label className="form-label">WHERE Conditions</label>
            {whereClauses.map((w, i) => (
              <div key={i} style={{ display: 'flex', gap: '0.5rem', marginBottom: '0.25rem' }}>
                <select className="form-select" value={w.column} onChange={e => {
                  const updated = [...whereClauses]; updated[i].column = e.target.value; setWhereClauses(updated);
                }} style={{ width: '150px' }}>
                  <option value="">Column...</option>
                  {columns.map(c => <option key={c} value={c}>{c}</option>)}
                </select>
                <select className="form-select" value={w.operator} onChange={e => {
                  const updated = [...whereClauses]; updated[i].operator = e.target.value; setWhereClauses(updated);
                }} style={{ width: '80px' }}>
                  <option value="=">=</option>
                  <option value="!=">!=</option>
                  <option value=">">&gt;</option>
                  <option value="<">&lt;</option>
                  <option value=">=">&gt;=</option>
                  <option value="<=">&lt;=</option>
                </select>
                <input className="form-input" value={w.value} onChange={e => {
                  const updated = [...whereClauses]; updated[i].value = e.target.value; setWhereClauses(updated);
                }} placeholder="Value" style={{ flex: 1 }} />
                <button className="btn btn-sm btn-danger" onClick={() => setWhereClauses(prev => prev.filter((_, j) => j !== i))}>X</button>
              </div>
            ))}
            <button className="btn btn-sm btn-secondary" onClick={() => setWhereClauses(prev => [...prev, { column: '', operator: '=', value: '' }])}>
              + Add Condition
            </button>
          </div>

          {/* ORDER BY */}
          <div style={{ marginBottom: '1rem' }}>
            <label className="form-label">ORDER BY</label>
            {orderByClauses.map((o, i) => (
              <div key={i} style={{ display: 'flex', gap: '0.5rem', marginBottom: '0.25rem' }}>
                <select className="form-select" value={o.column} onChange={e => {
                  const updated = [...orderByClauses]; updated[i].column = e.target.value; setOrderByClauses(updated);
                }} style={{ width: '150px' }}>
                  <option value="">Column...</option>
                  {columns.map(c => <option key={c} value={c}>{c}</option>)}
                </select>
                <select className="form-select" value={o.direction} onChange={e => {
                  const updated = [...orderByClauses]; updated[i].direction = e.target.value as 'ASC' | 'DESC'; setOrderByClauses(updated);
                }} style={{ width: '100px' }}>
                  <option value="ASC">ASC</option>
                  <option value="DESC">DESC</option>
                </select>
                <button className="btn btn-sm btn-danger" onClick={() => setOrderByClauses(prev => prev.filter((_, j) => j !== i))}>X</button>
              </div>
            ))}
            <button className="btn btn-sm btn-secondary" onClick={() => setOrderByClauses(prev => [...prev, { column: '', direction: 'ASC' }])}>
              + Add Order
            </button>
          </div>

          {/* Limit/Offset */}
          <div style={{ display: 'flex', gap: '1rem', marginBottom: '1rem' }}>
            <div>
              <label className="form-label">LIMIT</label>
              <input className="form-input" type="number" value={limit} onChange={e => setLimit(e.target.value)} style={{ width: '100px' }} />
            </div>
            <div>
              <label className="form-label">OFFSET</label>
              <input className="form-input" type="number" value={offset} onChange={e => setOffset(e.target.value)} style={{ width: '100px' }} />
            </div>
          </div>
        </div>
      </div>

      {/* Generated SQL */}
      <div className="card" style={{ marginBottom: '1rem' }}>
        <div className="card-header"><h3>Generated SQL</h3></div>
        <div className="card-body">
          <pre style={{ background: '#161822', padding: '1rem', borderRadius: '6px', color: '#6366f1',
            fontFamily: 'monospace', fontSize: '0.9rem', overflowX: 'auto', margin: 0 }}>
            {generatedSQL || '-- Select a table to start building your query'}
          </pre>
          <div style={{ marginTop: '0.75rem', display: 'flex', gap: '0.5rem' }}>
            <button className="btn btn-primary" onClick={executeQuery} disabled={!generatedSQL || executing}>
              {executing ? 'Executing...' : 'Execute Query'}
            </button>
            <button className="btn btn-secondary" onClick={() => {
              if (generatedSQL) navigator.clipboard.writeText(generatedSQL);
            }}>
              Copy SQL
            </button>
          </div>
        </div>
      </div>

      {/* Results */}
      {result && (
        <div className="card">
          <div className="card-header">
            <h3>Results {result.success ? `(${result.data?.rows?.length || 0} rows)` : '(Error)'}</h3>
          </div>
          <div className="card-body">
            {result.error && <div className="alert alert-error">{result.error}</div>}
            {result.message && !result.data && <div className="alert alert-success">{result.message}</div>}
            {result.data && (
              <div className="table-wrapper">
                <table className="data-table">
                  <thead>
                    <tr>{result.data.columns.map((c, i) => <th key={i}>{c}</th>)}</tr>
                  </thead>
                  <tbody>
                    {result.data.rows.map((row, i) => (
                      <tr key={i}>{row.map((cell, j) => <td key={j}>{cell}</td>)}</tr>
                    ))}
                  </tbody>
                </table>
              </div>
            )}
          </div>
        </div>
      )}
    </div>
  );
}
