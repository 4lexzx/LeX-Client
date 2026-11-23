import ReactDOM from 'react-dom/client';
import { App, StoreProvider } from './App';
import './styles/index.css';

const rootEl = document.getElementById('root');
if (!rootEl) throw new Error('#root no encontrado');

ReactDOM.createRoot(rootEl).render(
  <StoreProvider>
    <App />
  </StoreProvider>,
);
