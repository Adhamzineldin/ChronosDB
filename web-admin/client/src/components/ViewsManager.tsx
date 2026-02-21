import { useState, useEffect, useCallback } from 'react';
import { api } from '../api';

interface ViewsManagerProps {
  currentDb: string;
}

export default function ViewsManager({ currentDb }: ViewsManagerProps) {
  const [views, setViews] = useState<string[]>([]);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState('');
  const [success, setSuccess] = useState('');

  // Create modal
  const [showCreate, setShowCreate] = useState(false);
  const [viewName, setViewName] = useState('');
  const [viewQuery, setViewQuery] = useState('');
  const [creating, setCreating] = useState(false);

  // Preview modal
  const [previewView, setPreviewView] = useState<string | null>(null);
  const [previewData, setPreviewData] = useState<{ columns: string[]; rows: string[][] } | null>(null);
  const [previewLoading, setPreviewLoading] = useState(false);

  const loadViews = useCallback(async () => {
    if (!currentDb) return;
    setLoading(true);
    setError('');
    try {
      const result = await api.getViews();
      if (result.data?.rows) {
        setViews(result.data.rows.map(r => r[0]).filter(Boolean));
      } else {
        setViews([]);
      }
    } catch (err: any) {
      setError(err.message);
    } finally {
      setLoading(false);
    }
  }, [currentDb]);

  useEffect(() => {
    loadViews();
  }, [loadViews]);

  const handleCreate = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!viewName.trim() || !viewQuery.trim()) {
      setError('View name and SELECT query are required');
      return;
    }

    setCreating(true);
    setError('');
    setSuccess('');

    try {
      const result = await api.createView(viewName.trim(), viewQuery.trim());

      if (result.error) {
        setError(result.error);
      } else {
        setSuccess(`View "${viewName}" created successfully`);
        setViewName('');
        setViewQuery('');
        setShowCreate(false);
        await loadViews();
      }
    } catch (err: any) {
      setError(err.message);
    } finally {
      setCreating(false);
    }
  };

  const handleDrop = async (name: string) => {
    if (!confirm(`Drop view "${name}"? This cannot be undone.`)) return;

    setError('');
    setSuccess('');

    try {
      const result = await api.dropView(name);
      if (result.error) {
        setError(result.error);
      } else {
        setSuccess(`View "${name}" dropped`);
        await loadViews();
      }
    } catch (err: any) {
      setError(err.message);
    }
  };

  const handlePreview = async (name: string) => {
    setPreviewView(name);
    setPreviewLoading(true);
    setPreviewData(null);

    try {
      const result = await api.getViewData(name);
      if (result.data) {
        setPreviewData(result.data);
      } else if (result.error) {
        setError(result.error);
        setPreviewView(null);
      }
    } catch (err: any) {
      setError(err.message);
      setPreviewView(null);
    } finally {
      setPreviewLoading(false);
    }
  };

  const generateSQL = () => {
    if (!viewName.trim() || !viewQuery.trim()) return '';
    return `CREATE VIEW ${viewName.trim()} AS ${viewQuery.trim()}${viewQuery.trim().endsWith(';') ? '' : ';'}`;
  };

  if (!currentDb) {
    return (
      <div className="panel">
        <div className="panel-body">
          <p className="text-muted text-center">Select a database first to manage views.</p>
        </div>
      </div>
    );
  }

  return (
    <div className="views-manager">
      {error && (
        <div className="error-banner">
          <span>{error}</span>
          <button onClick={() => setError('')}>&times;</button>
        </div>
      )}
      {success && (
        <div className="success-banner">
          <span>{success}</span>
          <button onClick={() => setSuccess('')}>&times;</button>
        </div>
      )}

      {/* Views List */}
      <div className="panel">
        <div className="panel-header">
          <h3>Views in "{currentDb}"</h3>
          <button className="btn-sm btn-primary" onClick={() => setShowCreate(true)}>
            + Create View
          </button>
        </div>
        <div className="panel-body">
          {loading ? (
            <p className="text-muted">Loading views...</p>
          ) : views.length === 0 ? (
            <p className="text-muted text-center">
              No views found. Create one to get started.
            </p>
          ) : (
            <div className="table-list">
              {views.map((v) => (
                <div key={v} className="table-item">
                  <div style={{ display: 'flex', alignItems: 'center', flex: 1, cursor: 'pointer' }} onClick={() => handlePreview(v)}>
                    <span className="table-icon" style={{ color: '#8b5cf6' }}>&#9671;</span>
                    <span>{v}</span>
                  </div>
                  <div style={{ display: 'flex', gap: '0.5rem' }}>
                    <button
                      className="btn-sm btn-secondary"
                      onClick={(e) => { e.stopPropagation(); handlePreview(v); }}
                      title="Preview data"
                    >
                      Preview
                    </button>
                    <button
                      className="btn-sm btn-danger"
                      onClick={(e) => { e.stopPropagation(); handleDrop(v); }}
                      title="Drop view"
                    >
                      Drop
                    </button>
                  </div>
                </div>
              ))}
            </div>
          )}
        </div>
      </div>

      {/* Create View Modal */}
      {showCreate && (
        <div className="modal-overlay" onClick={() => setShowCreate(false)}>
          <div className="modal-content" style={{ maxWidth: '700px' }} onClick={(e) => e.stopPropagation()}>
            <div className="modal-header">
              <h3>Create View</h3>
              <button className="modal-close" onClick={() => setShowCreate(false)}>&times;</button>
            </div>
            <form onSubmit={handleCreate}>
              <div className="modal-body">
                <div className="form-group">
                  <label>View Name</label>
                  <input
                    type="text"
                    value={viewName}
                    onChange={(e) => setViewName(e.target.value)}
                    placeholder="e.g. active_users"
                    autoFocus
                  />
                </div>
                <div className="form-group">
                  <label>SELECT Query</label>
                  <textarea
                    value={viewQuery}
                    onChange={(e) => setViewQuery(e.target.value)}
                    placeholder="SELECT * FROM users WHERE active = 1"
                    rows={5}
                    style={{ fontFamily: 'monospace', fontSize: '0.9rem' }}
                  />
                </div>
                {generateSQL() && (
                  <div className="form-group">
                    <label>SQL Preview</label>
                    <div className="sql-preview">
                      <code>{generateSQL()}</code>
                    </div>
                  </div>
                )}
              </div>
              <div className="modal-footer">
                <button type="button" className="btn-secondary" onClick={() => setShowCreate(false)}>
                  Cancel
                </button>
                <button type="submit" className="btn-primary" disabled={creating || !viewName.trim() || !viewQuery.trim()}>
                  {creating ? 'Creating...' : 'Create View'}
                </button>
              </div>
            </form>
          </div>
        </div>
      )}

      {/* Preview Modal */}
      {previewView && (
        <div className="modal-overlay" onClick={() => setPreviewView(null)}>
          <div className="modal-content" style={{ maxWidth: '900px', maxHeight: '80vh' }} onClick={(e) => e.stopPropagation()}>
            <div className="modal-header">
              <h3>View: {previewView}</h3>
              <button className="modal-close" onClick={() => setPreviewView(null)}>&times;</button>
            </div>
            <div className="modal-body">
              {previewLoading ? (
                <p className="text-muted">Loading preview...</p>
              ) : previewData ? (
                <div className="data-table-wrapper" style={{ maxHeight: '50vh', overflow: 'auto' }}>
                  <table className="data-table">
                    <thead>
                      <tr>
                        {previewData.columns.map((col, i) => (
                          <th key={i}>{col}</th>
                        ))}
                      </tr>
                    </thead>
                    <tbody>
                      {previewData.rows.length === 0 ? (
                        <tr>
                          <td colSpan={previewData.columns.length} className="text-muted text-center">
                            No data returned
                          </td>
                        </tr>
                      ) : (
                        previewData.rows.map((row, ri) => (
                          <tr key={ri}>
                            {row.map((cell, ci) => (
                              <td key={ci}>{cell}</td>
                            ))}
                          </tr>
                        ))
                      )}
                    </tbody>
                  </table>
                </div>
              ) : (
                <p className="text-muted">No data available</p>
              )}
            </div>
            <div className="modal-footer">
              <button className="btn-secondary" onClick={() => setPreviewView(null)}>
                Close
              </button>
            </div>
          </div>
        </div>
      )}
    </div>
  );
}
