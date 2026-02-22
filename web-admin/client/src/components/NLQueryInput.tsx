import { useState, useEffect } from 'react';
import { api } from '../api';

interface Props {
  onGenerated: (sql: string) => void;
}

interface TableSchema {
  name: string;
  columns: string[];
}

function fuzzyMatch(input: string, target: string): boolean {
  return target.toLowerCase().includes(input.toLowerCase()) ||
    target.toLowerCase().replace(/_/g, '').includes(input.toLowerCase().replace(/\s+/g, ''));
}

function findTable(input: string, tables: TableSchema[]): TableSchema | null {
  // Exact match first
  for (const t of tables) {
    if (input.toLowerCase() === t.name.toLowerCase()) return t;
  }
  // Fuzzy match
  for (const t of tables) {
    if (fuzzyMatch(input, t.name)) return t;
  }
  // Plural/singular
  for (const t of tables) {
    if (fuzzyMatch(input + 's', t.name) || fuzzyMatch(input.replace(/s$/, ''), t.name)) return t;
  }
  return null;
}

function findColumn(input: string, table: TableSchema): string | null {
  for (const c of table.columns) {
    if (input.toLowerCase() === c.toLowerCase()) return c;
  }
  for (const c of table.columns) {
    if (fuzzyMatch(input, c)) return c;
  }
  return null;
}

function naturalLanguageToSQL(input: string, tables: TableSchema[]): string | null {
  const lower = input.toLowerCase().trim();
  if (!lower) return null;

  // Tokenize
  const words = lower.split(/\s+/);

  // Pattern: "count/how many <table>"
  if (lower.startsWith('how many ') || lower.startsWith('count ')) {
    const rest = lower.replace(/^(how many|count)\s+(of\s+|from\s+|in\s+)?/i, '');
    const table = findTable(rest.split(/\s/)[0], tables);
    if (table) return `SELECT COUNT(*) FROM ${table.name};`;
  }

  // Pattern: "average/mean of <col> in/from <table>"
  const avgMatch = lower.match(/(?:average|mean|avg)\s+(?:of\s+)?(\w+)\s+(?:in|from|of)\s+(\w+)/);
  if (avgMatch) {
    const table = findTable(avgMatch[2], tables);
    if (table) {
      const col = findColumn(avgMatch[1], table) || avgMatch[1];
      return `SELECT AVG(${col}) FROM ${table.name};`;
    }
  }

  // Pattern: "sum of <col> in/from <table>"
  const sumMatch = lower.match(/(?:sum|total)\s+(?:of\s+)?(\w+)\s+(?:in|from|of)\s+(\w+)/);
  if (sumMatch) {
    const table = findTable(sumMatch[2], tables);
    if (table) {
      const col = findColumn(sumMatch[1], table) || sumMatch[1];
      return `SELECT SUM(${col}) FROM ${table.name};`;
    }
  }

  // Pattern: "top N <table>"
  const topMatch = lower.match(/top\s+(\d+)\s+(\w+)/);
  if (topMatch) {
    const table = findTable(topMatch[2], tables);
    if (table) return `SELECT * FROM ${table.name} LIMIT ${topMatch[1]};`;
  }

  // Pattern: "distinct <col> from <table>"
  const distinctMatch = lower.match(/distinct\s+(\w+)\s+(?:from|in)\s+(\w+)/);
  if (distinctMatch) {
    const table = findTable(distinctMatch[2], tables);
    if (table) {
      const col = findColumn(distinctMatch[1], table) || distinctMatch[1];
      return `SELECT DISTINCT ${col} FROM ${table.name};`;
    }
  }

  // Pattern: "show/get/find/list [all] <table> [where/with <col> <op> <val>] [ordered/sorted by <col>]"
  const showMatch = lower.match(/(?:show|get|find|list|select|display)\s+(?:all\s+)?(?:from\s+)?(\w+)/);
  if (showMatch) {
    const table = findTable(showMatch[1], tables);
    if (table) {
      let sql = `SELECT * FROM ${table.name}`;

      // Check for WHERE condition
      const whereMatch = lower.match(/(?:where|with|having|whose)\s+(\w+)\s*(=|>|<|>=|<=|!=|is|equals?|greater|less)\s*(?:than\s+)?['"]?(\w+)['"]?/);
      if (whereMatch) {
        const col = findColumn(whereMatch[1], table) || whereMatch[1];
        let op = whereMatch[2];
        if (op === 'is' || op === 'equals' || op === 'equal') op = '=';
        if (op === 'greater') op = '>';
        if (op === 'less') op = '<';
        const val = isNaN(Number(whereMatch[3])) ? `'${whereMatch[3]}'` : whereMatch[3];
        sql += ` WHERE ${col} ${op} ${val}`;
      }

      // Check for ORDER BY
      const orderMatch = lower.match(/(?:order|sort|sorted|ordered)\s+(?:by\s+)?(\w+)(?:\s+(asc|desc))?/);
      if (orderMatch) {
        const col = findColumn(orderMatch[1], table) || orderMatch[1];
        sql += ` ORDER BY ${col}${orderMatch[2] ? ' ' + orderMatch[2].toUpperCase() : ''}`;
      }

      // Check for LIMIT
      const limitMatch = lower.match(/(?:limit|first|only)\s+(\d+)/);
      if (limitMatch) {
        sql += ` LIMIT ${limitMatch[1]}`;
      }

      return sql + ';';
    }
  }

  // Fallback: try to find any table name in the input
  for (const table of tables) {
    if (lower.includes(table.name.toLowerCase())) {
      return `SELECT * FROM ${table.name};`;
    }
  }

  return null;
}

export default function NLQueryInput({ onGenerated }: Props) {
  const [input, setInput] = useState('');
  const [tables, setTables] = useState<TableSchema[]>([]);
  const [suggestion, setSuggestion] = useState<string | null>(null);

  useEffect(() => {
    // Load table schemas for context
    api.getTables().then((res: any) => {
      if (res.data?.rows) {
        const tableNames = res.data.rows.map((r: string[]) => r[0]).filter(Boolean);
        Promise.all(tableNames.map((name: string) =>
          api.getTableSchema(name).then((schemaRes: any) => ({
            name,
            columns: schemaRes.data?.rows?.map((r: string[]) => r[0]).filter(Boolean) || []
          }))
        )).then(schemas => setTables(schemas));
      }
    });
  }, []);

  useEffect(() => {
    if (input.trim()) {
      const sql = naturalLanguageToSQL(input, tables);
      setSuggestion(sql);
    } else {
      setSuggestion(null);
    }
  }, [input, tables]);

  const handleUse = () => {
    if (suggestion) {
      onGenerated(suggestion);
      setInput('');
      setSuggestion(null);
    }
  };

  return (
    <div className="nl-query-input" style={{ marginBottom: '1rem' }}>
      <div style={{ display: 'flex', gap: '0.5rem', alignItems: 'center' }}>
        <div style={{ position: 'relative', flex: 1 }}>
          <input
            className="form-input"
            value={input}
            onChange={e => setInput(e.target.value)}
            onKeyDown={e => { if (e.key === 'Enter' && suggestion) handleUse(); }}
            placeholder="Type in natural language... e.g. 'show all users where age > 25'"
            style={{ width: '100%', paddingLeft: '2rem' }}
          />
          <span style={{ position: 'absolute', left: '0.6rem', top: '50%', transform: 'translateY(-50%)',
            color: '#6366f1', fontSize: '1rem' }}>NL</span>
        </div>
        <button className="btn btn-primary btn-sm" onClick={handleUse} disabled={!suggestion}
          style={{ whiteSpace: 'nowrap' }}>
          Use SQL
        </button>
      </div>
      {suggestion && (
        <div style={{ marginTop: '0.5rem', padding: '0.5rem 0.75rem', background: '#161822',
          borderRadius: '4px', border: '1px solid #2a2d3e', fontSize: '0.85rem' }}>
          <span style={{ color: '#6b6f82', marginRight: '0.5rem' }}>Generated:</span>
          <code style={{ color: '#6366f1' }}>{suggestion}</code>
        </div>
      )}
    </div>
  );
}
