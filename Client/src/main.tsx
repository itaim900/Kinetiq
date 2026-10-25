import React from 'react';
import ReactDOM from 'react-dom/client';
import App from './App';
import './theme.css';
import { applyTheme, getInitialTheme } from './lib/theme';

// index.html already set the theme; this keeps things right if that script
// didn't run (and doesn't save the device default as an explicit choice).
applyTheme(getInitialTheme(), false);

ReactDOM.createRoot(document.getElementById('root')!).render(
  <React.StrictMode>
    <App />
  </React.StrictMode>,
);
