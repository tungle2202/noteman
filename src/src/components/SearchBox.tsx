import { Search, X } from 'lucide-react';

export function SearchBox({
  value,
  onChange,
  placeholder,
}: {
  value: string;
  onChange: (v: string) => void;
  placeholder: string;
}) {
  return (
    <label className="search">
      <Search size={16} />
      <input
        value={value}
        onChange={(e) => onChange(e.target.value)}
        placeholder={placeholder}
        aria-label={placeholder}
      />
      {value && (
        <button
          type="button"
          onClick={(e) => {
            e.preventDefault();
            onChange('');
          }}
          style={{ display: 'grid', placeItems: 'center', color: 'var(--text-muted)', cursor: 'pointer' }}
          aria-label="Clear search"
        >
          <X size={14} />
        </button>
      )}
    </label>
  );
}
