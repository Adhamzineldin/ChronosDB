import { useState, useEffect, useRef } from 'react';
import { api } from '../api';

interface MetricPoint {
  time: string;
  value: number;
}

export default function RealtimeDashboard() {
  const [qps, setQps] = useState(0);
  const [qpsHistory, setQpsHistory] = useState<MetricPoint[]>([]);
  const [recentQueries, setRecentQueries] = useState<any[]>([]);
  const [bufferStats, setBufferStats] = useState<{ buffer_pool_size: number; pages_in_use: number } | null>(null);
  const [aiStatus, setAiStatus] = useState<any>(null);
  const [anomalies, setAnomalies] = useState<any[]>([]);
  const [blockedQueries, setBlockedQueries] = useState<any[]>([]);
  const intervalRef = useRef<ReturnType<typeof setInterval> | null>(null);

  const fetchAll = async () => {
    try {
      const [historyRes, bufferRes, aiRes, anomalyRes, blockedRes] = await Promise.all([
        api.getHistory(20),
        api.getBufferStats(),
        api.getAIDetailed(),
        api.getAnomalies(),
        api.getBlockedQueries(),
      ]);

      if (historyRes.success) {
        setQps(historyRes.qps || 0);
        setRecentQueries(historyRes.records || []);
        setQpsHistory(prev => {
          const now = new Date().toLocaleTimeString();
          const next = [...prev, { time: now, value: historyRes.qps || 0 }];
          return next.slice(-30); // Keep last 30 data points
        });
      }

      if (bufferRes.success) setBufferStats(bufferRes);
      if (aiRes) setAiStatus(aiRes);
      if ((anomalyRes as any).data?.rows) setAnomalies((anomalyRes as any).data.rows);
      if ((blockedRes as any).data?.rows) setBlockedQueries((blockedRes as any).data.rows);
    } catch (e) {
      // Silently fail on poll errors
    }
  };

  useEffect(() => {
    fetchAll();
    intervalRef.current = setInterval(fetchAll, 3000);
    return () => { if (intervalRef.current) clearInterval(intervalRef.current); };
  }, []);

  const maxQps = Math.max(...qpsHistory.map(p => p.value), 1);
  const bufferUsage = bufferStats ? ((bufferStats.pages_in_use / Math.max(bufferStats.buffer_pool_size, 1)) * 100) : 0;

  return (
    <div className="realtime-dashboard">
      {/* Top Stats Cards */}
      <div className="stats-grid" style={{ display: 'grid', gridTemplateColumns: 'repeat(4, 1fr)', gap: '1rem', marginBottom: '1.5rem' }}>
        <div className="card stat-card">
          <div className="card-body" style={{ textAlign: 'center', padding: '1.25rem' }}>
            <div style={{ fontSize: '2rem', fontWeight: 700, color: '#6366f1' }}>{qps.toFixed(1)}</div>
            <div style={{ color: '#9ca0b0', fontSize: '0.85rem' }}>Queries/sec</div>
          </div>
        </div>
        <div className="card stat-card">
          <div className="card-body" style={{ textAlign: 'center', padding: '1.25rem' }}>
            <div style={{ fontSize: '2rem', fontWeight: 700, color: '#10b981' }}>{bufferUsage.toFixed(1)}%</div>
            <div style={{ color: '#9ca0b0', fontSize: '0.85rem' }}>Buffer Pool Usage</div>
          </div>
        </div>
        <div className="card stat-card">
          <div className="card-body" style={{ textAlign: 'center', padding: '1.25rem' }}>
            <div style={{ fontSize: '2rem', fontWeight: 700, color: '#f59e0b' }}>
              {aiStatus?.learning_engine?.total_queries || 0}
            </div>
            <div style={{ color: '#9ca0b0', fontSize: '0.85rem' }}>AI Queries Observed</div>
          </div>
        </div>
        <div className="card stat-card">
          <div className="card-body" style={{ textAlign: 'center', padding: '1.25rem' }}>
            <div style={{ fontSize: '2rem', fontWeight: 700, color: '#ef4444' }}>
              {anomalies.length > 0 && anomalies[0]?.[0] !== '(none)' ? anomalies.length : 0}
            </div>
            <div style={{ color: '#9ca0b0', fontSize: '0.85rem' }}>Active Anomalies</div>
          </div>
        </div>
      </div>

      <div style={{ display: 'grid', gridTemplateColumns: '2fr 1fr', gap: '1rem', marginBottom: '1.5rem' }}>
        {/* QPS Chart (simple SVG bar chart) */}
        <div className="card">
          <div className="card-header"><h3>Query Throughput (queries/sec)</h3></div>
          <div className="card-body">
            <svg width="100%" height="180" viewBox={`0 0 ${qpsHistory.length * 20 + 10} 180`} preserveAspectRatio="none">
              {qpsHistory.map((point, i) => {
                const barHeight = (point.value / maxQps) * 150;
                return (
                  <g key={i}>
                    <rect x={i * 20 + 5} y={160 - barHeight} width="14" height={barHeight}
                      fill="#6366f1" rx="2" opacity={0.8} />
                    <title>{point.time}: {point.value.toFixed(1)} qps</title>
                  </g>
                );
              })}
              <line x1="0" y1="160" x2={qpsHistory.length * 20 + 10} y2="160" stroke="#2a2d3e" strokeWidth="1" />
            </svg>
            <div style={{ display: 'flex', justifyContent: 'space-between', color: '#6b6f82', fontSize: '0.75rem' }}>
              <span>{qpsHistory[0]?.time || ''}</span>
              <span>{qpsHistory[qpsHistory.length - 1]?.time || ''}</span>
            </div>
          </div>
        </div>

        {/* Buffer Pool Gauge */}
        <div className="card">
          <div className="card-header"><h3>Buffer Pool</h3></div>
          <div className="card-body" style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center' }}>
            <svg width="140" height="140" viewBox="0 0 140 140">
              <circle cx="70" cy="70" r="60" fill="none" stroke="#1e2130" strokeWidth="12" />
              <circle cx="70" cy="70" r="60" fill="none" stroke={bufferUsage > 80 ? '#ef4444' : bufferUsage > 50 ? '#f59e0b' : '#10b981'}
                strokeWidth="12" strokeDasharray={`${bufferUsage * 3.77} 377`}
                strokeLinecap="round" transform="rotate(-90 70 70)" />
              <text x="70" y="65" textAnchor="middle" fill="#e4e6ef" fontSize="22" fontWeight="700">{bufferUsage.toFixed(0)}%</text>
              <text x="70" y="85" textAnchor="middle" fill="#9ca0b0" fontSize="11">Usage</text>
            </svg>
            {bufferStats && (
              <div style={{ marginTop: '0.5rem', color: '#9ca0b0', fontSize: '0.8rem', textAlign: 'center' }}>
                {bufferStats.pages_in_use.toLocaleString()} / {bufferStats.buffer_pool_size.toLocaleString()} pages
              </div>
            )}
          </div>
        </div>
      </div>

      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '1rem' }}>
        {/* Recent Queries */}
        <div className="card">
          <div className="card-header"><h3>Recent Queries</h3></div>
          <div className="card-body" style={{ maxHeight: '300px', overflow: 'auto' }}>
            <table className="data-table" style={{ fontSize: '0.8rem' }}>
              <thead>
                <tr><th>SQL</th><th>Time (ms)</th><th>Status</th></tr>
              </thead>
              <tbody>
                {recentQueries.slice(0, 15).map((q, i) => (
                  <tr key={i}>
                    <td style={{ maxWidth: '300px', overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>
                      {q.sql}
                    </td>
                    <td>{typeof q.elapsed_ms === 'number' ? q.elapsed_ms.toFixed(2) : q.elapsed_ms}</td>
                    <td><span style={{ color: q.success ? '#10b981' : '#ef4444' }}>{q.success ? 'OK' : 'ERR'}</span></td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        </div>

        {/* AI & Security Alerts */}
        <div className="card">
          <div className="card-header"><h3>Security & Anomaly Alerts</h3></div>
          <div className="card-body" style={{ maxHeight: '300px', overflow: 'auto' }}>
            {blockedQueries.length > 0 && blockedQueries[0]?.[0] !== '0' && (
              <div style={{ marginBottom: '1rem' }}>
                <h4 style={{ color: '#ef4444', fontSize: '0.85rem', marginBottom: '0.5rem' }}>Blocked Queries</h4>
                {blockedQueries.slice(0, 5).map((bq, i) => (
                  <div key={i} style={{ padding: '0.5rem', background: '#1e1215', borderRadius: '4px', marginBottom: '0.25rem', fontSize: '0.8rem' }}>
                    <span style={{ color: '#ef4444' }}>[{bq[3]}]</span> {bq[1]?.substring(0, 60)}...
                    <span style={{ color: '#6b6f82' }}> by {bq[2]}</span>
                  </div>
                ))}
              </div>
            )}
            {anomalies.length > 0 && anomalies[0]?.[0] !== '(none)' ? (
              anomalies.slice(0, 8).map((a, i) => (
                <div key={i} style={{ padding: '0.5rem', background: '#161822', borderRadius: '4px', marginBottom: '0.25rem', fontSize: '0.8rem',
                  borderLeft: `3px solid ${a[1] === 'HIGH' ? '#ef4444' : a[1] === 'MEDIUM' ? '#f59e0b' : '#10b981'}` }}>
                  <strong>{a[0]}</strong> - {a[1]} (z-score: {a[2]})
                </div>
              ))
            ) : (
              <p style={{ color: '#6b6f82', textAlign: 'center', padding: '2rem 0' }}>No anomalies detected. System is healthy.</p>
            )}
          </div>
        </div>
      </div>
    </div>
  );
}
