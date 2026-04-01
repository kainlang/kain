export type KainChatSeedMessage = {
  role: string;
  text: string;
};

export type KainCardEntry = {
  kicker?: string | null;
  title?: string | null;
  body?: string | null;
  summary?: string | null;
};

export type KainMetric = {
  value?: string | null;
  label?: string | null;
};

export type KainProcessStep = {
  title?: string | null;
  body?: string | null;
};

export type KainExperienceCatalogEntry = {
  id?: string | null;
  mode?: string | null;
  page_title?: string | null;
  output_slug?: string | null;
  theme?: string | null;
  content?: string | null;
  scene?: string | null;
};

export type KainAppModule = {
  name: string;
  route?: string | null;
  summary?: string | null;
  tags?: string[] | null;
};

export type KainRealtimeChannel = {
  name: string;
  protocol?: string | null;
  cadence?: string | null;
  summary?: string | null;
  producer?: string | null;
  tags?: string[] | null;
};

export type KainRuntimeRoutes = {
  chat?: string | null;
  chat_stream?: string | null;
  chat_ws?: string | null;
  chat_tools?: string | null;
  chat_tool_events?: string | null;
  realtime_stream?: string | null;
  realtime_ws?: string | null;
  uploads?: string | null;
  uploads_prefix?: string | null;
  analytics_event?: string | null;
  analytics_events?: string | null;
  actors_dispatch?: string | null;
  actors_events?: string | null;
};

export type KainRuntimeStorage = {
  root?: string | null;
  submissions?: string | null;
  uploads?: string | null;
  analytics?: string | null;
  auth?: string | null;
  chat?: string | null;
  actors?: string | null;
  tools?: string | null;
};

export type KainRuntimeConfig = {
  host?: string | null;
  port?: number | null;
  client_features?: string[] | null;
  routes?: KainRuntimeRoutes | null;
  storage?: KainRuntimeStorage | null;
};

export type KainAuthMethod = {
  label: string;
  detail?: string | null;
  scope?: string | null;
  status?: string | null;
};

export type KainAuthDescriptor = {
  kicker?: string | null;
  title?: string | null;
  body?: string | null;
  session_title?: string | null;
  session_body?: string | null;
  methods?: KainAuthMethod[] | null;
};

export type KainIdentityDescriptor = {
  kicker?: string | null;
  title?: string | null;
  body?: string | null;
  providers?: KainCardEntry[] | null;
  roles?: KainCardEntry[] | null;
  policies?: KainCardEntry[] | null;
};

export type KainBillingDescriptor = {
  kicker?: string | null;
  title?: string | null;
  body?: string | null;
  plans?: KainCardEntry[] | null;
  invoices?: KainCardEntry[] | null;
  taxes?: KainCardEntry[] | null;
};

export type KainSubscriptionDescriptor = {
  kicker?: string | null;
  title?: string | null;
  body?: string | null;
  tiers?: KainCardEntry[] | null;
};

export type KainCmsDescriptor = {
  kicker?: string | null;
  title?: string | null;
  body?: string | null;
  content_types?: KainCardEntry[] | null;
  workflow?: KainCardEntry[] | null;
};

export type KainMediaLibraryDescriptor = {
  kicker?: string | null;
  title?: string | null;
  body?: string | null;
  libraries?: KainCardEntry[] | null;
  pipelines?: KainCardEntry[] | null;
};

export type KainAutomationDescriptor = {
  kicker?: string | null;
  title?: string | null;
  body?: string | null;
  flows?: KainCardEntry[] | null;
};

export type KainWebhookDescriptor = {
  kicker?: string | null;
  title?: string | null;
  body?: string | null;
  events?: KainCardEntry[] | null;
};

export type KainApiEndpoint = {
  method?: string | null;
  path?: string | null;
  purpose?: string | null;
};

export type KainApiReferenceDescriptor = {
  kicker?: string | null;
  title?: string | null;
  body?: string | null;
  endpoints?: KainApiEndpoint[] | null;
};

export type KainDeveloperPortalDescriptor = {
  kicker?: string | null;
  title?: string | null;
  body?: string | null;
  tools?: KainCardEntry[] | null;
};

export type KainSeoDescriptor = {
  kicker?: string | null;
  title?: string | null;
  body?: string | null;
  targets?: KainCardEntry[] | null;
};

export type KainAgentDescriptor = {
  kicker?: string | null;
  title?: string | null;
  body?: string | null;
  agents?: KainCardEntry[] | null;
  tools?: KainCardEntry[] | null;
  workflows?: KainProcessStep[] | null;
};

export type KainCommerceOffer = {
  kicker?: string | null;
  cadence?: string | null;
  name: string;
  price?: string | null;
  summary?: string | null;
  features?: string[] | null;
  actions?: { label: string; href: string; style?: string }[] | null;
};

export type KainCommerceDescriptor = {
  offers?: KainCommerceOffer[] | null;
};

export type KainUploadsDescriptor = {
  kicker?: string | null;
  title?: string | null;
  body?: string | null;
};

export type KainAnalyticsDescriptor = {
  kicker?: string | null;
  title?: string | null;
  body?: string | null;
};

export type KainSceneLayer = {
  title?: string | null;
  summary?: string | null;
  color?: string | null;
};

export type KainSceneDescriptor = {
  kicker?: string | null;
  title?: string | null;
  summary?: string | null;
  layers?: KainSceneLayer[] | null;
};

export type KainStatusService = {
  name: string;
  status?: string | null;
  detail?: string | null;
  uptime?: string | null;
};

export type KainStatusIncident = {
  phase?: string | null;
  title?: string | null;
  body?: string | null;
  started_at?: string | null;
  resolved_at?: string | null;
};

export type KainStatusDescriptor = {
  kicker?: string | null;
  title?: string | null;
  summary?: string | null;
  updated_at?: string | null;
  services?: KainStatusService[] | null;
  incidents?: KainStatusIncident[] | null;
};

export type KainRoadmapItem = {
  phase?: string | null;
  title?: string | null;
  body?: string | null;
  eta?: string | null;
};

export type KainTeamMember = {
  name: string;
  role?: string | null;
  summary?: string | null;
  focus?: string | null;
};

export type KainSupportChannel = {
  name: string;
  detail?: string | null;
  availability?: string | null;
  href?: string | null;
};

export type KainLegalLink = {
  kicker?: string | null;
  title?: string | null;
  summary?: string | null;
  href?: string | null;
  label?: string | null;
};

export type KainSecurityControl = {
  title?: string | null;
  detail?: string | null;
  status?: string | null;
};

export type KainSecurityDescriptor = {
  kicker?: string | null;
  title?: string | null;
  body?: string | null;
  controls?: KainSecurityControl[] | null;
};

export type KainCommunityChannel = {
  name?: string | null;
  platform?: string | null;
  summary?: string | null;
  members?: string | null;
  cadence?: string | null;
  href?: string | null;
};

export type KainCommunityDescriptor = {
  kicker?: string | null;
  title?: string | null;
  body?: string | null;
  channels?: KainCommunityChannel[] | null;
};

export type KainEventEntry = {
  title?: string | null;
  date?: string | null;
  format?: string | null;
  summary?: string | null;
  focus?: string | null;
  href?: string | null;
};

export type KainEventsDescriptor = {
  kicker?: string | null;
  title?: string | null;
  body?: string | null;
  rsvp_form_id?: string | null;
  upcoming?: KainEventEntry[] | null;
};

export type KainNewsletterDescriptor = {
  kicker?: string | null;
  title?: string | null;
  body?: string | null;
  cadence?: string | null;
  topics?: string[] | null;
  form_id?: string | null;
};

export type KainComplianceControl = {
  title?: string | null;
  detail?: string | null;
  status?: string | null;
};

export type KainComplianceDescriptor = {
  kicker?: string | null;
  title?: string | null;
  body?: string | null;
  controls?: KainComplianceControl[] | null;
};

export type KainObservabilitySignal = {
  title?: string | null;
  detail?: string | null;
  owner?: string | null;
  cadence?: string | null;
};

export type KainObservabilityDescriptor = {
  kicker?: string | null;
  title?: string | null;
  body?: string | null;
  signals?: KainObservabilitySignal[] | null;
};

export type KainInfrastructureItem = {
  title?: string | null;
  detail?: string | null;
  tier?: string | null;
  status?: string | null;
};

export type KainInfrastructureDescriptor = {
  kicker?: string | null;
  title?: string | null;
  body?: string | null;
  stack?: KainInfrastructureItem[] | null;
};

export type KainLocalizationLanguage = {
  name?: string | null;
  coverage?: string | null;
  status?: string | null;
};

export type KainLocalizationRegion = {
  name?: string | null;
  timezone?: string | null;
  status?: string | null;
};

export type KainLocalizationDescriptor = {
  kicker?: string | null;
  title?: string | null;
  body?: string | null;
  languages?: KainLocalizationLanguage[] | null;
  regions?: KainLocalizationRegion[] | null;
};

export type KainAccessibilityCheck = {
  title?: string | null;
  detail?: string | null;
  status?: string | null;
};

export type KainAccessibilityDescriptor = {
  kicker?: string | null;
  title?: string | null;
  body?: string | null;
  checks?: KainAccessibilityCheck[] | null;
};

export type KainPerformanceTarget = {
  title?: string | null;
  detail?: string | null;
  target?: string | null;
};

export type KainPerformanceDescriptor = {
  kicker?: string | null;
  title?: string | null;
  body?: string | null;
  targets?: KainPerformanceTarget[] | null;
};

export type KainGrowthCampaign = {
  title?: string | null;
  channel?: string | null;
  summary?: string | null;
  status?: string | null;
};

export type KainGrowthFunnel = {
  stage?: string | null;
  metric?: string | null;
  owner?: string | null;
};

export type KainGrowthDescriptor = {
  kicker?: string | null;
  title?: string | null;
  body?: string | null;
  campaigns?: KainGrowthCampaign[] | null;
  funnels?: KainGrowthFunnel[] | null;
};

export type KainExperimentTest = {
  name?: string | null;
  hypothesis?: string | null;
  status?: string | null;
  metric?: string | null;
  owner?: string | null;
};

export type KainExperimentDescriptor = {
  kicker?: string | null;
  title?: string | null;
  body?: string | null;
  tests?: KainExperimentTest[] | null;
};

export type KainServiceEntry = {
  name?: string | null;
  tier?: string | null;
  summary?: string | null;
  sla?: string | null;
  owner?: string | null;
};

export type KainServiceCatalog = {
  kicker?: string | null;
  title?: string | null;
  body?: string | null;
  services?: KainServiceEntry[] | null;
};

export type KainSuccessPlaybook = {
  title?: string | null;
  goal?: string | null;
  owner?: string | null;
  cadence?: string | null;
};

export type KainSuccessDescriptor = {
  kicker?: string | null;
  title?: string | null;
  body?: string | null;
  playbooks?: KainSuccessPlaybook[] | null;
};

export type KainNotificationChannel = {
  name?: string | null;
  purpose?: string | null;
  owner?: string | null;
  cadence?: string | null;
  transport?: string | null;
};

export type KainNotificationDescriptor = {
  kicker?: string | null;
  title?: string | null;
  body?: string | null;
  channels?: KainNotificationChannel[] | null;
};

export type KainReleaseNote = {
  version?: string | null;
  date?: string | null;
  summary?: string | null;
  owner?: string | null;
  highlights?: string[] | null;
};

export type KainReleaseNotesDescriptor = {
  kicker?: string | null;
  title?: string | null;
  body?: string | null;
  entries?: KainReleaseNote[] | null;
};

export type KainFeatureFlag = {
  name?: string | null;
  status?: string | null;
  owner?: string | null;
  impact?: string | null;
  summary?: string | null;
};

export type KainFeatureFlagsDescriptor = {
  kicker?: string | null;
  title?: string | null;
  body?: string | null;
  flags?: KainFeatureFlag[] | null;
};

export type KainIncidentPlaybook = {
  severity?: string | null;
  title?: string | null;
  summary?: string | null;
  owner?: string | null;
  sla?: string | null;
  body?: string | null;
};

export type KainIncidentResponseDescriptor = {
  kicker?: string | null;
  title?: string | null;
  body?: string | null;
  playbooks?: KainIncidentPlaybook[] | null;
};

export type KainIncidentHistoryEntry = {
  phase?: string | null;
  title?: string | null;
  body?: string | null;
};

export type KainCrmStage = {
  stage?: string | null;
  goal?: string | null;
  owner?: string | null;
  sla?: string | null;
  summary?: string | null;
  detail?: string | null;
};

export type KainCrmPipelineDescriptor = {
  kicker?: string | null;
  title?: string | null;
  body?: string | null;
  stages?: KainCrmStage[] | null;
};

export type KainActorTopologyNode = {
  id?: string | null;
  name?: string | null;
  role?: string | null;
  channel?: string | null;
};

export type KainActorTopologyEdge = {
  from?: string | null;
  to?: string | null;
  relation?: string | null;
  detail?: string | null;
};

export type KainActorTopology = {
  kicker?: string | null;
  title?: string | null;
  body?: string | null;
  nodes?: KainActorTopologyNode[] | null;
  edges?: KainActorTopologyEdge[] | null;
};

export type KainCareerRole = {
  title?: string | null;
  location?: string | null;
  type?: string | null;
  summary?: string | null;
  href?: string | null;
  tags?: string[] | null;
};

export type KainCareersDescriptor = {
  kicker?: string | null;
  title?: string | null;
  body?: string | null;
  roles?: KainCareerRole[] | null;
};

export type KainPartner = {
  name: string;
  category?: string | null;
  detail?: string | null;
  href?: string | null;
};

export type KainPressAsset = {
  label?: string | null;
  detail?: string | null;
  href?: string | null;
};

export type KainPressContact = {
  name?: string | null;
  role?: string | null;
  email?: string | null;
};

export type KainPressKit = {
  kicker?: string | null;
  title?: string | null;
  body?: string | null;
  assets?: KainPressAsset[] | null;
  contacts?: KainPressContact[] | null;
};

export type KainSiteData = {
  brand?: string;
  runtime?: KainRuntimeConfig;
  experience_catalog?: KainExperienceCatalogEntry[];
  nav?: { label: string; href: string }[];
  hero?: {
    kicker?: string;
    title?: string;
    body?: string;
    actions?: { label: string; href: string; style?: string }[];
  };
  app_modules?: KainAppModule[];
  realtime_channels?: KainRealtimeChannel[];
  chat_seed?: KainChatSeedMessage[];
  chat_personas?: KainCardEntry[];
  chat_modes?: KainCardEntry[];
  chat_playbooks?: KainProcessStep[];
  chat_tools?: KainCardEntry[];
  chat_memory?: KainCardEntry[];
  actor_playbooks?: KainProcessStep[];
  actor_tools?: KainCardEntry[];
  actor_topology?: KainActorTopology | null;
  actor_policies?: KainCardEntry[];
  actor_metrics?: KainMetric[];
  actor_supervision?: KainCardEntry[];
  actor_queues?: KainCardEntry[];
  actor_jobs?: KainCardEntry[];
  actor_schedules?: KainCardEntry[];
  actor_hosts?: KainCardEntry[];
  scene?: KainSceneDescriptor;
  auth?: KainAuthDescriptor | null;
  identity?: KainIdentityDescriptor | null;
  tenant_management?: KainCardEntry[] | null;
  sso_stack?: KainCardEntry[] | null;
  api_key_management?: KainCardEntry[] | null;
  identity_verification?: KainCardEntry[] | null;
  fraud_risk?: KainCardEntry[] | null;
  consent_center?: KainCardEntry[] | null;
  audit_logs?: KainCardEntry[] | null;
  data_exports?: KainCardEntry[] | null;
  marketplace_stack?: KainCardEntry[] | null;
  integration_marketplace?: KainCardEntry[] | null;
  content_syndication?: KainCardEntry[] | null;
  billing?: KainBillingDescriptor | null;
  subscriptions?: KainSubscriptionDescriptor | null;
  cms?: KainCmsDescriptor | null;
  media_library?: KainMediaLibraryDescriptor | null;
  scene_pipeline?: KainCardEntry[];
  render_stack?: KainCardEntry[];
  interaction_modes?: KainCardEntry[];
  device_profiles?: KainCardEntry[];
  scene_assets?: KainCardEntry[];
  material_library?: KainCardEntry[];
  lighting_rigs?: KainCardEntry[];
  camera_rigs?: KainCardEntry[];
  animation_stack?: KainCardEntry[];
  physics_stack?: KainCardEntry[];
  spatial_audio?: KainCardEntry[];
  xr_modes?: KainCardEntry[];
  shader_stack?: KainCardEntry[];
  streaming_stack?: KainCardEntry[];
  automation?: KainAutomationDescriptor | null;
  webhooks?: KainWebhookDescriptor | null;
  api_reference?: KainApiReferenceDescriptor | null;
  developer_portal?: KainDeveloperPortalDescriptor | null;
  seo_stack?: KainSeoDescriptor | null;
  brand_system?: KainCardEntry[] | null;
  creative_systems?: KainCardEntry[] | null;
  copy_deck?: KainCardEntry[] | null;
  content_models?: KainCardEntry[] | null;
  editorial_workflow?: KainProcessStep[] | null;
  email_templates?: KainCardEntry[] | null;
  campaign_briefs?: KainCardEntry[] | null;
  icon_system?: KainCardEntry[] | null;
  motion_library?: KainCardEntry[] | null;
  illustration_library?: KainCardEntry[] | null;
  social_presence?: KainCardEntry[] | null;
  content_calendar?: KainProcessStep[] | null;
  release_pipeline?: KainProcessStep[] | null;
  qa_program?: KainCardEntry[] | null;
  domain_stack?: KainCardEntry[] | null;
  trust_center?: KainCardEntry[] | null;
  ai_agents?: KainAgentDescriptor | null;
  knowledge_sources?: KainCardEntry[];
  memory_stores?: KainCardEntry[];
  tool_registry?: KainCardEntry[];
  agent_workflows?: KainProcessStep[];
  model_stack?: KainCardEntry[];
  voice_stack?: KainCardEntry[];
  moderation_stack?: KainCardEntry[];
  ai_evaluations?: KainCardEntry[];
  ai_guardrails?: KainCardEntry[];
  prompt_library?: KainCardEntry[];
  ui_components?: KainCardEntry[];
  ui_layouts?: KainCardEntry[];
  ui_tokens?: KainCardEntry[];
  frontend_stack?: KainCardEntry[];
  ui_runtime?: KainCardEntry[];
  kain_ui_stack?: KainCardEntry[];
  ui_state_stack?: KainCardEntry[];
  ui_routing_stack?: KainCardEntry[];
  ui_data_stack?: KainCardEntry[];
  ui_form_stack?: KainCardEntry[];
  ui_motion_stack?: KainCardEntry[];
  ui_testing_stack?: KainCardEntry[];
  ui_tooling_stack?: KainCardEntry[];
  chat_runtime?: KainCardEntry[];
  actor_runtime?: KainCardEntry[];
  ffi_bridges?: KainCardEntry[];
  kain_script_stack?: KainCardEntry[];
  client_runtime_stack?: KainCardEntry[];
  server_runtime_stack?: KainCardEntry[];
  commerce?: KainCommerceDescriptor | null;
  uploads?: KainUploadsDescriptor | null;
  analytics?: KainAnalyticsDescriptor | null;
  analytics_stack?: KainCardEntry[] | null;
  attribution_stack?: KainCardEntry[] | null;
  data_warehouse?: KainCardEntry[] | null;
  cdp_stack?: KainCardEntry[] | null;
  event_bus?: KainCardEntry[] | null;
  data_pipelines?: KainCardEntry[] | null;
  status?: KainStatusDescriptor | null;
  roadmap?: KainRoadmapItem[] | null;
  team_members?: KainTeamMember[] | null;
  support_channels?: KainSupportChannel[] | null;
  legal?: KainLegalLink[] | null;
  security?: KainSecurityDescriptor | null;
  community?: KainCommunityDescriptor | null;
  events?: KainEventsDescriptor | null;
  newsletter?: KainNewsletterDescriptor | null;
  compliance?: KainComplianceDescriptor | null;
  compliance_frameworks?: KainCardEntry[] | null;
  data_governance?: KainCardEntry[];
  backup_plan?: KainCardEntry[];
  observability?: KainObservabilityDescriptor | null;
  infrastructure?: KainInfrastructureDescriptor | null;
  edge_runtime?: KainCardEntry[];
  worker_runtime?: KainCardEntry[];
  api_gateway?: KainCardEntry[];
  rate_limits?: KainCardEntry[];
  cache_stack?: KainCardEntry[];
  search_stack?: KainCardEntry[];
  storage_stack?: KainCardEntry[];
  session_store?: KainCardEntry[];
  runtime_hosts?: KainCardEntry[];
  deployment_targets?: KainCardEntry[];
  localization?: KainLocalizationDescriptor | null;
  accessibility?: KainAccessibilityDescriptor | null;
  performance?: KainPerformanceDescriptor | null;
  enablement_programs?: KainCardEntry[];
  onboarding_flows?: KainProcessStep[];
  data_retention?: KainCardEntry[];
  reliability_slos?: KainCardEntry[];
  incident_history?: KainIncidentHistoryEntry[];
  growth?: KainGrowthDescriptor | null;
  experiments?: KainExperimentDescriptor | null;
  service_catalog?: KainServiceCatalog | null;
  success?: KainSuccessDescriptor | null;
  notifications?: KainNotificationDescriptor | null;
  release_notes?: KainReleaseNotesDescriptor | null;
  feature_flags?: KainFeatureFlagsDescriptor | null;
  incident_response?: KainIncidentResponseDescriptor | null;
  ops_runbooks?: KainCardEntry[] | null;
  crm_pipeline?: KainCrmPipelineDescriptor | null;
  careers?: KainCareersDescriptor | null;
  partners?: KainPartner[] | null;
  press_kit?: KainPressKit | null;
};

export function resolveAgainstLocation(relativeOrAbsolute: string): string {
  try {
    return new URL(relativeOrAbsolute, window.location.href).toString();
  } catch {
    return relativeOrAbsolute;
  }
}

export async function fetchJson<T>(url: string): Promise<T> {
  const response = await fetch(url, { headers: { accept: "application/json" } });
  if (!response.ok) {
    throw new Error(`fetch failed: ${response.status} ${response.statusText}`);
  }
  return (await response.json()) as T;
}

export async function loadSiteData(siteDataPath: string): Promise<KainSiteData> {
  return fetchJson<KainSiteData>(resolveAgainstLocation(siteDataPath));
}
